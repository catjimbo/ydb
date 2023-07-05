#include "topic_readwrite_scenario.h"

#include <ydb/public/lib/ydb_cli/commands/topic_workload/topic_workload_defines.h>
#include <ydb/public/lib/ydb_cli/commands/topic_workload/topic_workload_describe.h>
#include <ydb/public/lib/ydb_cli/commands/topic_workload/topic_workload_reader.h>
#include <ydb/public/lib/ydb_cli/commands/topic_workload/topic_workload_writer.h>
#include <ydb/public/lib/ydb_cli/commands/ydb_common.h>

#define INCLUDE_YDB_INTERNAL_H
#include <ydb/public/sdk/cpp/client/impl/ydb_internal/logger/log.h>
#undef INCLUDE_YDB_INTERNAL_H

#include <util/generic/guid.h>

namespace NYdb::NConsoleClient {

using TConfig = TClientCommand::TConfig;

TTopicOperationsScenario::TTopicOperationsScenario() :
    ErrorFlag(std::make_shared<std::atomic_bool>())
{
}

int TTopicOperationsScenario::Run(const TConfig& config)
{
    InitLog(config);
    InitDriver(config);
    InitStatsCollector();

    return DoRun(config);
}

void TTopicOperationsScenario::EnsurePercentileIsValid() const
{
    if (Percentile > 100 || Percentile <= 0) {
        throw TMisuseException() << "--percentile should be in range (0,100].";
    }
}

void TTopicOperationsScenario::EnsureWarmupSecIsValid() const
{
    if (WarmupSec >= TotalSec) {
        throw TMisuseException() << "--warmup should be less than --seconds.";
    }
}

THolder<TLogBackend> TTopicOperationsScenario::MakeLogBackend(TConfig::EVerbosityLevel level)
{
    return CreateLogBackend("cerr",
                            TConfig::VerbosityLevelToELogPriority(level));
}

void TTopicOperationsScenario::InitLog(const TConfig& config)
{
    Log = std::make_shared<TLog>(MakeLogBackend(config.VerbosityLevel));
    Log->SetFormatter(GetPrefixLogFormatter(""));
}

void TTopicOperationsScenario::InitDriver(const TConfig& config)
{
    Driver =
        std::make_unique<NYdb::TDriver>(TYdbCommand::CreateDriver(config,
                                                                  MakeLogBackend(config.VerbosityLevel)));
}

void TTopicOperationsScenario::InitStatsCollector()
{
    StatsCollector =
        std::make_shared<TTopicWorkloadStatsCollector>(ProducerThreadCount,
                                                       ConsumerCount * ConsumerThreadCount,
                                                       Quiet,
                                                       PrintTimestamp,
                                                       WindowSec.Seconds(),
                                                       TotalSec.Seconds(),
                                                       WarmupSec.Seconds(),
                                                       Percentile,
                                                       ErrorFlag);
}

void TTopicOperationsScenario::CreateTopic(const TString& database,
                                           const TString& topic,
                                           ui32 partitionCount,
                                           ui32 consumerCount)
{
    auto topicPath =
        TCommandWorkloadTopicDescribe::GenerateFullTopicName(database, topic);

    EnsureTopicNotExist(topicPath);
    CreateTopic(topicPath, partitionCount, consumerCount);
}

void TTopicOperationsScenario::DropTopic(const TString& database,
                                         const TString& topic)
{
    Y_VERIFY(Driver);

    NTopic::TTopicClient client(*Driver);
    auto topicPath =
        TCommandWorkloadTopicDescribe::GenerateFullTopicName(database, topic);

    auto result = client.DropTopic(topicPath).GetValueSync();
    ThrowOnError(result);
}

void TTopicOperationsScenario::EnsureTopicNotExist(const TString& topic)
{
    Y_VERIFY(Driver);

    NTopic::TTopicClient client(*Driver);

    auto result = client.DescribeTopic(topic, {}).GetValueSync();

    if (result.GetTopicDescription().GetTotalPartitionsCount() != 0) {
        ythrow yexception() << "Topic '" << topic << "' already exists.";
    }
}

void TTopicOperationsScenario::CreateTopic(const TString& topic,
                                           ui32 partitionCount,
                                           ui32 consumerCount)
{
    Y_VERIFY(Driver);

    NTopic::TTopicClient client(*Driver);

    NTopic::TCreateTopicSettings settings;
    settings.PartitioningSettings(partitionCount, partitionCount);

    for (unsigned consumerIdx = 0; consumerIdx < consumerCount; ++consumerIdx) {
        settings
            .BeginAddConsumer(TCommandWorkloadTopicDescribe::GenerateConsumerName(consumerIdx))
            .EndAddConsumer();
    }

    auto result = client.CreateTopic(topic, settings).GetValueSync();
    ThrowOnError(result);
}

void TTopicOperationsScenario::StartConsumerThreads(std::vector<std::future<void>>& threads,
                                                    const TString& database)
{
    auto count = std::make_shared<std::atomic_uint>();

    for (ui32 consumerIdx = 0, readerIdx = 0; consumerIdx < ConsumerCount; ++consumerIdx) {
        for (ui32 threadIdx = 0; threadIdx < ConsumerThreadCount; ++threadIdx, ++readerIdx) {
            TTopicWorkloadReaderParams readerParams{
                .TotalSec = TotalSec.Seconds(),
                .Driver = *Driver,
                .Log = Log,
                .StatsCollector = StatsCollector,
                .ErrorFlag = ErrorFlag,
                .StartedCount = count,
                .Database = database,
                .TopicName = TopicName,
                .ConsumerIdx = consumerIdx,
                .ReaderIdx = readerIdx
            };

            threads.push_back(std::async([readerParams = std::move(readerParams)]() mutable { TTopicWorkloadReader::ReaderLoop(readerParams); }));
        }
    }

    while (*count != ConsumerThreadCount * ConsumerCount) {
        Sleep(TDuration::MilliSeconds(10));
    }
}

void TTopicOperationsScenario::StartProducerThreads(std::vector<std::future<void>>& threads,
                                                    ui32 partitionCount,
                                                    ui32 partitionSeed,
                                                    const std::vector<TString>& generatedMessages)
{
    auto count = std::make_shared<std::atomic_uint>();
    for (ui32 writerIdx = 0; writerIdx < ProducerThreadCount; ++writerIdx) {
        TTopicWorkloadWriterParams writerParams{
            .TotalSec = TotalSec.Seconds(),
            .WarmupSec = WarmupSec.Seconds(),
            .Driver = *Driver,
            .Log = Log,
            .StatsCollector = StatsCollector,
            .ErrorFlag = ErrorFlag,
            .StartedCount = count,
            .GeneratedMessages = generatedMessages,
            .TopicName = TopicName,
            .ByteRate = MessageRate != 0 ? MessageRate * MessageSize : ByteRate,
            .MessageSize = MessageSize,
            .ProducerThreadCount = ProducerThreadCount,
            .WriterIdx = writerIdx,
            .ProducerId = TGUID::CreateTimebased().AsGuidString(),
            .PartitionId = (partitionSeed + writerIdx) % partitionCount,
            .Codec = Codec
        };

        threads.push_back(std::async([writerParams = std::move(writerParams)]() mutable { TTopicWorkloadWriterWorker::WriterLoop(writerParams); }));
    }

    while (*count != ProducerThreadCount) {
        Sleep(TDuration::MilliSeconds(10));
    }
}

void TTopicOperationsScenario::JoinThreads(const std::vector<std::future<void>>& threads)
{
    for (auto& future : threads) {
        future.wait();
    }

    WRITE_LOG(Log, ELogPriority::TLOG_INFO, "All thread joined.");
}

bool TTopicOperationsScenario::AnyErrors() const
{
    if (!*ErrorFlag) {
        return false;
    }

    WRITE_LOG(Log, ELogPriority::TLOG_EMERG, "Problems occured while processing messages.");

    return true;
}

bool TTopicOperationsScenario::AnyIncomingMessages() const
{
    if (StatsCollector->GetTotalReadMessages()) {
        return true;
    }

    WRITE_LOG(Log, ELogPriority::TLOG_EMERG, "No messages were read.");

    return false;
}

bool TTopicOperationsScenario::AnyOutgoingMessages() const
{
    if (StatsCollector->GetTotalWriteMessages()) {
        return true;
    }

    WRITE_LOG(Log, ELogPriority::TLOG_EMERG, "No messages were written.");

    return false;
}

}
