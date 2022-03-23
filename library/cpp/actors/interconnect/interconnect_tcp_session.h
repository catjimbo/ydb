#pragma once

#include <library/cpp/actors/core/hfunc.h>
#include <library/cpp/actors/core/event_pb.h>
#include <library/cpp/actors/core/events.h>
#include <library/cpp/actors/core/log.h>
#include <library/cpp/actors/helpers/mon_histogram_helper.h>
#include <library/cpp/actors/protos/services_common.pb.h>
#include <library/cpp/actors/util/datetime.h>
#include <library/cpp/actors/util/rope.h>
#include <library/cpp/actors/util/funnel_queue.h>
#include <library/cpp/actors/util/recentwnd.h>
#include <library/cpp/monlib/dynamic_counters/counters.h>
#include <library/cpp/actors/core/actor_bootstrapped.h>

#include <util/generic/queue.h>
#include <util/generic/deque.h>
#include <util/datetime/cputimer.h>

#include "interconnect_impl.h"
#include "poller_tcp.h"
#include "poller_actor.h"
#include "interconnect_channel.h"
#include "logging.h"
#include "watchdog_timer.h"
#include "event_holder_pool.h"
#include "channel_scheduler.h"

#include <unordered_set>
#include <unordered_map>

namespace NActors {
    class TSlowPathChecker {
        using TTraceCallback = std::function<void(double)>;
        TTraceCallback Callback;
        const NHPTimer::STime Start;

    public:
        TSlowPathChecker(TTraceCallback&& callback)
            : Callback(std::move(callback))
            , Start(GetCycleCountFast())
        {
        }

        ~TSlowPathChecker() {
            const NHPTimer::STime end = GetCycleCountFast();
            const NHPTimer::STime elapsed = end - Start;
            if (elapsed > 1000000) {
                Callback(NHPTimer::GetSeconds(elapsed) * 1000);
            }
        }

        operator bool() const {
            return false;
        }
    };

#define LWPROBE_IF_TOO_LONG(...)                                               \
    if (auto __x = TSlowPathChecker{[&](double ms) { LWPROBE(__VA_ARGS__); }}) \
        ;                                                                      \
    else

    class TTimeLimit {
    public:
        TTimeLimit(ui64 limitInCycles)
            : UpperLimit(limitInCycles == 0 ? 0 : GetCycleCountFast() + limitInCycles)
        {
        }

        TTimeLimit(ui64 startTS, ui64 limitInCycles)
            : UpperLimit(limitInCycles == 0 ? 0 : startTS + limitInCycles)
        {
        }

        bool CheckExceeded() {
            return UpperLimit != 0 && GetCycleCountFast() > UpperLimit;
        }

        const ui64 UpperLimit;
    };

    static constexpr TDuration DEFAULT_DEADPEER_TIMEOUT = TDuration::Seconds(10);
    static constexpr TDuration DEFAULT_LOST_CONNECTION_TIMEOUT = TDuration::Seconds(10);
    static constexpr ui32 DEFAULT_MAX_INFLIGHT_DATA = 10240 * 1024;
    static constexpr ui32 DEFAULT_TOTAL_INFLIGHT_DATA = 4 * 10240 * 1024;

    class TInterconnectProxyTCP;

    enum class EUpdateState : ui8 {
        NONE,                 // no updates generated by input session yet
        INFLIGHT,             // one update is inflight, and no more pending
        INFLIGHT_AND_PENDING, // one update is inflight, and one is pending
        CONFIRMING,           // confirmation inflight
    };

    struct TReceiveContext: public TAtomicRefCount<TReceiveContext> {
        /* All invokations to these fields should be thread-safe */

        ui64 ControlPacketSendTimer = 0;
        ui64 ControlPacketId = 0;

        // number of packets received by input session
        TAtomic PacketsReadFromSocket = 0;
        TAtomic DataPacketsReadFromSocket = 0;

        // last processed packet by input session
        std::atomic_uint64_t LastProcessedPacketSerial = 0;
        static constexpr uint64_t LastProcessedPacketSerialLockBit = uint64_t(1) << 63;

        // for hardened checks
        TAtomic NumInputSessions = 0;

        NHPTimer::STime StartTime;

        std::atomic<ui64> PingRTT_us = 0;
        std::atomic<i64> ClockSkew_us = 0;

        std::atomic<EUpdateState> UpdateState;
        static_assert(std::atomic<EUpdateState>::is_always_lock_free);

        bool WriteBlockedByFullSendBuffer = false;
        bool ReadPending = false;

        std::array<TRope, 16> ChannelArray;
        std::unordered_map<ui16, TRope> ChannelMap;

        TReceiveContext() {
            GetTimeFast(&StartTime);
        }

        // returns false if sessions needs to be terminated and packet not to be processed
        bool AdvanceLastProcessedPacketSerial() {
            for (;;) {
                uint64_t value = LastProcessedPacketSerial.load();
                if (value & LastProcessedPacketSerialLockBit) {
                    return false;
                }
                if (LastProcessedPacketSerial.compare_exchange_weak(value, value + 1)) {
                    return true;
                }
            }
        }

        ui64 LockLastProcessedPacketSerial() {
            for (;;) {
                uint64_t value = LastProcessedPacketSerial.load();
                if (value & LastProcessedPacketSerialLockBit) {
                    return value & ~LastProcessedPacketSerialLockBit;
                }
                if (LastProcessedPacketSerial.compare_exchange_strong(value, value | LastProcessedPacketSerialLockBit)) {
                    return value;
                }
            }
        }

        void UnlockLastProcessedPacketSerial() {
            LastProcessedPacketSerial = LastProcessedPacketSerial.load() & ~LastProcessedPacketSerialLockBit;
        }

        ui64 GetLastProcessedPacketSerial() {
            return LastProcessedPacketSerial.load() & ~LastProcessedPacketSerialLockBit;
        }
    };

    class TInputSessionTCP
       : public TActorBootstrapped<TInputSessionTCP>
       , public TInterconnectLoggingBase
    {
        enum {
            EvCheckDeadPeer = EventSpaceBegin(TEvents::ES_PRIVATE),
            EvResumeReceiveData,
        };

        struct TEvCheckDeadPeer : TEventLocal<TEvCheckDeadPeer, EvCheckDeadPeer> {};
        struct TEvResumeReceiveData : TEventLocal<TEvResumeReceiveData, EvResumeReceiveData> {};

    public:
        static constexpr EActivityType ActorActivityType() {
            return INTERCONNECT_SESSION_TCP;
        }

        TInputSessionTCP(const TActorId& sessionId,
                         TIntrusivePtr<NInterconnect::TStreamSocket> socket,
                         TIntrusivePtr<TReceiveContext> context,
                         TInterconnectProxyCommon::TPtr common,
                         std::shared_ptr<IInterconnectMetrics> metrics,
                         ui32 nodeId,
                         ui64 lastConfirmed,
                         TDuration deadPeerTimeout,
                         TSessionParams params);

    private:
        friend class TActorBootstrapped<TInputSessionTCP>;

        void Bootstrap();

        STRICT_STFUNC(WorkingState,
            cFunc(TEvents::TSystem::PoisonPill, PassAway)
            hFunc(TEvPollerReady, Handle)
            hFunc(TEvPollerRegisterResult, Handle)
            cFunc(EvResumeReceiveData, HandleResumeReceiveData)
            cFunc(TEvInterconnect::TEvCloseInputSession::EventType, CloseInputSession)
            cFunc(EvCheckDeadPeer, HandleCheckDeadPeer)
            cFunc(TEvConfirmUpdate::EventType, HandleConfirmUpdate)
        )

    private:
        TRope IncomingData;

        const TActorId SessionId;
        TIntrusivePtr<NInterconnect::TStreamSocket> Socket;
        TPollerToken::TPtr PollerToken;
        TIntrusivePtr<TReceiveContext> Context;
        TInterconnectProxyCommon::TPtr Common;
        const ui32 NodeId;
        const TSessionParams Params;

        // header we are currently processing (parsed from the stream)
        union {
            TTcpPacketHeader_v1 v1;
            TTcpPacketHeader_v2 v2;
            char Data[1];
        } Header;
        ui64 HeaderConfirm, HeaderSerial;

        size_t PayloadSize;
        ui32 ChecksumExpected, Checksum;
        bool IgnorePayload;
        TRope Payload;
        enum class EState {
            HEADER,
            PAYLOAD,
        };
        EState State = EState::HEADER;

        THolder<TEvUpdateFromInputSession> UpdateFromInputSession;

        ui64 ConfirmedByInput;

        std::shared_ptr<IInterconnectMetrics> Metrics;

        bool CloseInputSessionRequested = false;

        void CloseInputSession();

        void Handle(TEvPollerReady::TPtr ev);
        void Handle(TEvPollerRegisterResult::TPtr ev);
        void HandleResumeReceiveData();
        void HandleConfirmUpdate();
        void ReceiveData();
        void ProcessHeader(size_t headerLen);
        void ProcessPayload(ui64& numDataBytes);
        void ProcessEvent(TRope& data, TEventDescr& descr);
        bool ReadMore();

        void ReestablishConnection(TDisconnectReason reason);
        void DestroySession(TDisconnectReason reason);
        void PassAway() override;

        TDeque<TIntrusivePtr<TRopeAlignedBuffer>> Buffers;

        static constexpr size_t NumPreallocatedBuffers = 16;
        void PreallocateBuffers();

        inline ui64 GetMaxCyclesPerEvent() const {
            return DurationToCycles(TDuration::MicroSeconds(500));
        }

        const TDuration DeadPeerTimeout;
        TInstant LastReceiveTimestamp;
        void HandleCheckDeadPeer();

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // pinger logic

        bool NewPingProtocol = false;
        TDeque<TDuration> PingQ; // last N ping samples
        TDeque<i64> SkewQ; // last N calculated clock skew samples

        void HandlePingResponse(TDuration passed);
        void HandleClock(TInstant clock);
    };

    class TInterconnectSessionTCP
       : public TActor<TInterconnectSessionTCP>
       , public TInterconnectLoggingBase
    {
        enum {
            EvCheckCloseOnIdle = EventSpaceBegin(TEvents::ES_PRIVATE),
            EvCheckLostConnection,
            EvRam,
            EvTerminate,
            EvFreeItems,
        };

        struct TEvCheckCloseOnIdle : TEventLocal<TEvCheckCloseOnIdle, EvCheckCloseOnIdle> {};
        struct TEvCheckLostConnection : TEventLocal<TEvCheckLostConnection, EvCheckLostConnection> {};

        struct TEvRam : TEventLocal<TEvRam, EvRam> {
            const bool Batching;
            TEvRam(bool batching) : Batching(batching) {}
        };

        struct TEvTerminate : TEventLocal<TEvTerminate, EvTerminate> {
            TDisconnectReason Reason;

            TEvTerminate(TDisconnectReason reason)
                : Reason(std::move(reason))
            {}
        };

        const TInstant Created;
        TInstant NewConnectionSet;
        ui64 MessagesGot = 0;
        ui64 MessagesWrittenToBuffer = 0;
        ui64 PacketsGenerated = 0;
        ui64 PacketsWrittenToSocket = 0;
        ui64 PacketsConfirmed = 0;

    public:
        static constexpr EActivityType ActorActivityType() {
            return INTERCONNECT_SESSION_TCP;
        }

        TInterconnectSessionTCP(TInterconnectProxyTCP* const proxy, TSessionParams params);
        ~TInterconnectSessionTCP();

        void Init();
        void CloseInputSession();

        static TEvTerminate* NewEvTerminate(TDisconnectReason reason) {
            return new TEvTerminate(std::move(reason));
        }

        TDuration GetPingRTT() const {
            return TDuration::MicroSeconds(ReceiveContext->PingRTT_us);
        }

        i64 GetClockSkew() const {
            return ReceiveContext->ClockSkew_us;
        }

    private:
        friend class TInterconnectProxyTCP;

        void Handle(TEvTerminate::TPtr& ev);
        void HandlePoison();
        void Terminate(TDisconnectReason reason);
        void PassAway() override;

        void Forward(STATEFN_SIG);
        void Subscribe(STATEFN_SIG);
        void Unsubscribe(STATEFN_SIG);

        STRICT_STFUNC(StateFunc,
            fFunc(TEvInterconnect::EvForward, Forward)
            cFunc(TEvents::TEvPoisonPill::EventType, HandlePoison)
            fFunc(TEvInterconnect::TEvConnectNode::EventType, Subscribe)
            fFunc(TEvents::TEvSubscribe::EventType, Subscribe)
            fFunc(TEvents::TEvUnsubscribe::EventType, Unsubscribe)
            cFunc(TEvFlush::EventType, HandleFlush)
            hFunc(TEvPollerReady, Handle)
            hFunc(TEvPollerRegisterResult, Handle)
            hFunc(TEvUpdateFromInputSession, Handle)
            hFunc(TEvRam, HandleRam)
            hFunc(TEvCheckCloseOnIdle, CloseOnIdleWatchdog)
            hFunc(TEvCheckLostConnection, LostConnectionWatchdog)
            cFunc(TEvents::TSystem::Wakeup, SendUpdateToWhiteboard)
            hFunc(TEvSocketDisconnect, OnDisconnect)
            hFunc(TEvTerminate, Handle)
            hFunc(TEvProcessPingRequest, Handle)
        )

        void Handle(TEvUpdateFromInputSession::TPtr& ev);

        void OnDisconnect(TEvSocketDisconnect::TPtr& ev);

        THolder<TEvHandshakeAck> ProcessHandshakeRequest(TEvHandshakeAsk::TPtr& ev);
        void SetNewConnection(TEvHandshakeDone::TPtr& ev);

        TEvRam* RamInQueue = nullptr;
        ui64 RamStartedCycles = 0;
        void HandleRam(TEvRam::TPtr& ev);
        void GenerateTraffic();

        void SendUpdateToWhiteboard(bool connected = true);
        ui32 CalculateQueueUtilization();

        void Handle(TEvPollerReady::TPtr& ev);
        void Handle(TEvPollerRegisterResult::TPtr ev);
        void WriteData();

        ui64 MakePacket(bool data, TMaybe<ui64> pingMask = {});
        void FillSendingBuffer(TTcpPacketOutTask& packet, ui64 serial);
        bool DropConfirmed(ui64 confirm);
        void ShutdownSocket(TDisconnectReason reason);

        void StartHandshake();
        void ReestablishConnection(TEvHandshakeDone::TPtr&& ev, bool startHandshakeOnSessionClose,
                TDisconnectReason reason);
        void ReestablishConnectionWithHandshake(TDisconnectReason reason);
        void ReestablishConnectionExecute();

        TInterconnectProxyTCP* const Proxy;

        // various connection settings access
        TDuration GetDeadPeerTimeout() const;
        TDuration GetCloseOnIdleTimeout() const;
        TDuration GetLostConnectionTimeout() const;
        ui32 GetTotalInflightAmountOfData() const;
        ui64 GetMaxCyclesPerEvent() const;


        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // pinger

        TInstant LastPingTimestamp;
        static constexpr TDuration PingPeriodicity = TDuration::Seconds(1);
        void IssuePingRequest();
        void Handle(TEvProcessPingRequest::TPtr ev);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        TInstant LastInputActivityTimestamp;
        TInstant LastPayloadActivityTimestamp;
        TWatchdogTimer<TEvCheckCloseOnIdle> CloseOnIdleWatchdog;
        TWatchdogTimer<TEvCheckLostConnection> LostConnectionWatchdog;

        void OnCloseOnIdleTimerHit() {
            LOG_INFO_IC("ICS27", "CloseOnIdle timer hit, session terminated");
            Terminate(TDisconnectReason::CloseOnIdle());
        }

        void OnLostConnectionTimerHit() {
            LOG_ERROR_IC("ICS28", "LostConnection timer hit, session terminated");
            Terminate(TDisconnectReason::LostConnection());
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        const TSessionParams Params;
        TMaybe<TEventHolderPool> Pool;
        TMaybe<TChannelScheduler> ChannelScheduler;
        ui64 TotalOutputQueueSize;
        bool OutputStuckFlag;
        TRecentWnd<std::pair<ui64, ui64>> OutputQueueUtilization;
        size_t NumEventsInReadyChannels = 0;

        void SetOutputStuckFlag(bool state);
        void SwitchStuckPeriod();

        using TSendQueue = TList<TTcpPacketOutTask>;
        TSendQueue SendQueue;
        TSendQueue SendQueueCache;
        TSendQueue::iterator SendQueuePos;
        ui64 WriteBlockedCycles = 0; // start of current block period
        TDuration WriteBlockedTotal; // total incremental duration that session has been blocked
        ui64 BytesUnwritten = 0;

        void TrimSendQueueCache();

        TDuration GetWriteBlockedTotal() const {
            if (ReceiveContext->WriteBlockedByFullSendBuffer) {
                double blockedUs = NHPTimer::GetSeconds(GetCycleCountFast() - WriteBlockedCycles) * 1000000.0;
                return WriteBlockedTotal + TDuration::MicroSeconds(blockedUs); // append current blocking period if any
            } else {
                return WriteBlockedTotal;
            }
        }

        ui64 OutputCounter;
        ui64 LastSentSerial = 0;

        TInstant LastHandshakeDone;

        TIntrusivePtr<NInterconnect::TStreamSocket> Socket;
        TPollerToken::TPtr PollerToken;
        ui32 SendBufferSize;
        ui64 InflightDataAmount = 0;

        std::unordered_map<TActorId, ui64, TActorId::THash> Subscribers;

        // time at which we want to send confirmation packet even if there was no outgoing data
        ui64 UnconfirmedBytes = 0;
        TInstant ForcePacketTimestamp = TInstant::Max();
        TPriorityQueue<TInstant, TVector<TInstant>, std::greater<TInstant>> FlushSchedule;
        size_t MaxFlushSchedule = 0;
        ui64 FlushEventsScheduled = 0;
        ui64 FlushEventsProcessed = 0;

        void SetForcePacketTimestamp(TDuration period);
        void ScheduleFlush();
        void HandleFlush();
        void ResetFlushLogic();

        void GenerateHttpInfo(TStringStream& str);

        TIntrusivePtr<TReceiveContext> ReceiveContext;
        TActorId ReceiverId;
        TDuration Ping;

        ui64 ConfirmPacketsForcedBySize = 0;
        ui64 ConfirmPacketsForcedByTimeout = 0;

        ui64 LastConfirmed = 0;

        TEvHandshakeDone::TPtr PendingHandshakeDoneEvent;
        bool StartHandshakeOnSessionClose = false;

        ui64 EqualizeCounter = 0;
    };

    class TInterconnectSessionKiller
       : public TActorBootstrapped<TInterconnectSessionKiller> {
        ui32 RepliesReceived = 0;
        ui32 RepliesNumber = 0;
        TActorId LargestSession = TActorId();
        ui64 MaxBufferSize = 0;
        TInterconnectProxyCommon::TPtr Common;

    public:
        static constexpr EActivityType ActorActivityType() {
            return INTERCONNECT_SESSION_KILLER;
        }

        TInterconnectSessionKiller(TInterconnectProxyCommon::TPtr common)
            : Common(common)
        {
        }

        void Bootstrap() {
            auto sender = SelfId();
            const auto eventFabric = [&sender](const TActorId& recp) -> IEventHandle* {
                auto ev = new TEvSessionBufferSizeRequest();
                return new IEventHandle(recp, sender, ev, IEventHandle::FlagTrackDelivery);
            };
            RepliesNumber = TlsActivationContext->ExecutorThread.ActorSystem->BroadcastToProxies(eventFabric);
            Become(&TInterconnectSessionKiller::StateFunc);
        }

        STRICT_STFUNC(StateFunc,
            hFunc(TEvSessionBufferSizeResponse, ProcessResponse)
            cFunc(TEvents::TEvUndelivered::EventType, ProcessUndelivered)
        )

        void ProcessResponse(TEvSessionBufferSizeResponse::TPtr& ev) {
            RepliesReceived++;
            if (MaxBufferSize < ev->Get()->BufferSize) {
                MaxBufferSize = ev->Get()->BufferSize;
                LargestSession = ev->Get()->SessionID;
            }
            if (RepliesReceived == RepliesNumber) {
                Send(LargestSession, new TEvents::TEvPoisonPill);
                AtomicUnlock(&Common->StartedSessionKiller);
                PassAway();
            }
        }

        void ProcessUndelivered() {
            RepliesReceived++;
        }
    };

    void CreateSessionKillingActor(TInterconnectProxyCommon::TPtr common);

}
