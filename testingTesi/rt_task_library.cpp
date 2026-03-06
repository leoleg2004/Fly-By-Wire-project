#include "rt_task_library.hpp"

#include <algorithm>
#include <cmath>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <stdexcept>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/core/ReturnCode.hpp>

#include "TaskData.hpp"
#include "TaskDataPubSubTypes.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#define handle_error(en, msg)                                                   \
  if ((en) != 0) {                                                             \
    errno = (en);                                                              \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  }

// Utilities -----------------------------------------------------------------
static void timespec_add_ms(struct timespec *t, long ms) {
  t->tv_sec += ms / 1000;
  t->tv_nsec += (ms % 1000) * 1000000;
  if (t->tv_nsec >= 1000000000) {
    t->tv_sec++;
    t->tv_nsec -= 1000000000;
  }
}

static uint64_t time_to_ms(struct timespec t) {
  return static_cast<uint64_t>(t.tv_sec) * 1000 + (t.tv_nsec / 1000000);
}

static uint64_t time_current_millisecs() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return time_to_ms(t);
}

// Pure CPU workload copied from app09
static void Activity(int parameter) {
  volatile double result = 0;
  for (long i = 0; i < parameter * 1000 * 1000; i++) {
    result = result + sin(i) + cos(i) + i * tan(i);
  }
  (void)result;
}

struct DDSContext {
  eprosima::fastdds::dds::DomainParticipant *participant{nullptr};
  eprosima::fastdds::dds::Publisher *publisher{nullptr};
  eprosima::fastdds::dds::Subscriber *subscriber{nullptr};
  eprosima::fastdds::dds::Topic *topic{nullptr};
  eprosima::fastdds::dds::DataWriter *writer{nullptr};
  eprosima::fastdds::dds::DataReader *reader{nullptr};
  eprosima::fastdds::dds::TypeSupport type;
};

static bool InitDDS(DDSContext &ctx, const std::string &participant_name) {
  using namespace eprosima::fastdds::dds;

  DomainParticipantQos pqos;
  pqos.name(participant_name.c_str());

  ctx.participant =
      DomainParticipantFactory::get_instance()->create_participant(1, pqos);
  if (!ctx.participant) {
    std::cerr << "DDS: errore creazione participant\n";
    return false;
  }

  ctx.type = TypeSupport(new TaskDataPubSubType());
  ctx.type.register_type(ctx.participant);

  ctx.topic = ctx.participant->create_topic("TaskDataTopic",
                                            ctx.type.get_type_name(),
                                            TOPIC_QOS_DEFAULT);
  if (!ctx.topic) {
    std::cerr << "DDS: errore creazione topic\n";
    return false;
  }

  ctx.publisher = ctx.participant->create_publisher(PUBLISHER_QOS_DEFAULT);
  ctx.subscriber = ctx.participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
  if (!ctx.publisher || !ctx.subscriber) {
    std::cerr << "DDS: errore creazione publisher/subscriber\n";
    return false;
  }

  ctx.writer = ctx.publisher->create_datawriter(ctx.topic, DATAWRITER_QOS_DEFAULT);
  ctx.reader = ctx.subscriber->create_datareader(ctx.topic, DATAREADER_QOS_DEFAULT);
  if (!ctx.writer || !ctx.reader) {
    std::cerr << "DDS: errore creazione writer/reader\n";
    return false;
  }

  std::cout << "DDS pronto: dominio 1, topic TaskDataTopic, participant '"
            << pqos.name() << "'\n";
  return true;
}

// Thread routine -------------------------------------------------------------
static void *PeriodicTask(void *ptr) {
  TaskConfig *task = static_cast<TaskConfig *>(ptr);

  struct timespec exec_release_time;
  clock_gettime(CLOCK_MONOTONIC, &exec_release_time);

  bool skip = false;

  for (int i = 0; i < task->iterations_to_run; i++) {
    timespec_add_ms(&exec_release_time, task->period_ms);
    uint64_t exec_next_release_time = time_to_ms(exec_release_time);

    uint64_t exec_start_time = time_current_millisecs();
    bool skipped_this_job = skip;

    if (!skip) {
      if (task->use_dds) {
        using namespace eprosima::fastdds::dds;
        auto *writer = static_cast<DataWriter *>(task->dds_writer);
        auto *reader = static_cast<DataReader *>(task->dds_reader);

        static thread_local uint32_t packet_id = 0;

        if (task->is_publisher && writer) {
          TaskData sample;
          sample.packet_id = ++packet_id;
          sample.workload = task->parameter;
          sample.deadline_missed = false;
          sample.latency_ms = 0.0f;
          writer->write(&sample);
          Activity(task->parameter);
        } else if (!task->is_publisher && reader) {
          TaskData sample;
          SampleInfo info;
          bool got = false;
          while (reader->take_next_sample(&sample, &info) ==
                 eprosima::fastdds::dds::RETCODE_OK) {
            if (info.valid_data) {
              got = true;
            }
          }

          if (got) {
            Activity(task->parameter);
          } else {
            Activity(std::max(1, task->parameter / 2));
          }
        } else {
          Activity(task->parameter);
        }
      } else {
        Activity(task->parameter);
      }
    }

    uint64_t exec_end_time = time_current_millisecs();
    uint64_t computational_cost = exec_end_time - exec_start_time;

    bool missed_now = false;
    if (skip) {
      skip = false;
    } else if (exec_end_time > exec_next_release_time) {
      skip = true;
      missed_now = true;
    }

    printf("%s:            exec_start_time         = %ld millisecs\n",
           task->name.c_str(), static_cast<long>(exec_start_time));
    printf("%s:            exec_end_time           = %ld millisecs\n",
           task->name.c_str(), static_cast<long>(exec_end_time));
    if (skipped_this_job) {
      printf("%s:   *SKIP*   cost                    =    %ld millisecs\n",
             task->name.c_str(), static_cast<long>(computational_cost));
    } else if (missed_now) {
      printf("%s:   -MISS-   cost                    = %ld millisecs\n",
             task->name.c_str(), static_cast<long>(computational_cost));
    } else {
      printf("%s:   DO JOB   cost                    = %ld millisecs\n",
             task->name.c_str(), static_cast<long>(computational_cost));
    }
    printf("%s:            period                  = %ld millisecs\n",
           task->name.c_str(), task->period_ms);
    printf("%s:            exec_next_release_time  = %ld millisecs\n\n",
           task->name.c_str(), static_cast<long>(exec_next_release_time));

    ActivityResult res{};
    res.iteration = i;
    res.start_time_ms = static_cast<long>(exec_start_time);
    res.end_time_ms = static_cast<long>(exec_end_time);
    res.cost_ms = static_cast<long>(computational_cost);
    res.missed_deadline = missed_now;
    res.skipped = skipped_this_job;
    task->results.push_back(res);

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &exec_release_time, NULL);
  }

  return nullptr;
}

// XML parsing ---------------------------------------------------------------
static void ParseTaskConfigFromXML(const std::string &xml_file, TaskConfig &t1,
                                   TaskConfig &t2) {
  boost::property_tree::ptree pt;
  boost::property_tree::read_xml(xml_file, pt);

  int idx = 0;
  for (const auto &kv : pt.get_child("AppConfig.Tasks")) {
    if (kv.first != "Task")
      continue;

    TaskConfig *target = (idx == 0) ? &t1 : &t2;
    target->name = kv.second.get<std::string>("Name", idx == 0 ? "Activity_1"
                                                                : "Activity_2");
    target->period_ms = kv.second.get<long>("Period_ms");
    target->parameter = kv.second.get<int>("Parameter", 10);
    target->deadline_ms = kv.second.get<long>("Deadline_ms", target->period_ms);

    idx++;
    if (idx >= 2)
      break;
  }

  if (idx < 2) {
    throw std::runtime_error("XML non contiene due Task completi");
  }
}

// CSV writer ---------------------------------------------------------------
static void ExportResultsToCSV(const std::string &filename,
                               const std::string &configuration,
                               const std::vector<TaskConfig> &tasks,
                               const std::string &config_xml_path) {
  namespace fs = std::filesystem;

  const fs::path base_dir = fs::absolute(fs::path{config_xml_path}).parent_path();
  const fs::path csv_dir = base_dir / "CSVData";
  std::error_code ec;
  fs::create_directories(csv_dir, ec);
  if (ec) {
    std::cerr << "Errore creazione cartella CSVData: " << ec.message() << "\n";
    return;
  }

  const fs::path target = csv_dir / filename;

  std::ofstream csv_file(target);
  if (!csv_file.is_open()) {
    std::cerr << "Errore creazione file CSV: " << target.string() << "\n";
    return;
  }

  csv_file << "Configuration,Task,Period(ms),Parameter,Deadline(ms),Iteration,";
  csv_file << "StartTime(ms),EndTime(ms),Cost(ms),MissedDeadline,Skipped\n";

  for (const auto &t : tasks) {
    for (const auto &res : t.results) {
      csv_file << configuration << ',' << t.name << ',' << t.period_ms << ','
               << t.parameter << ',' << t.deadline_ms << ',' << res.iteration
               << ',' << res.start_time_ms << ',' << res.end_time_ms << ','
               << res.cost_ms << ',' << (res.missed_deadline ? 1 : 0) << ','
               << (res.skipped ? 1 : 0) << "\n";
    }
  }

  csv_file.close();
  std::cout << "Dati esportati in: " << target.string() << "\n";
}

// Orchestrator -------------------------------------------------------------
static int RunTestVariant(const std::string &label, const std::string &policy,
                          const std::string &affinity,
                          const std::string &config_xml, bool use_dds,
                          DDSContext *dds_ctx) {
  TaskConfig task1{}, task2{};

  try {
    ParseTaskConfigFromXML(config_xml, task1, task2);
  } catch (const std::exception &e) {
    std::cerr << "Errore file XML: " << e.what() << "\n";
    return 1;
  }

  const long duration_ms = 20000;
  task1.iterations_to_run =
      std::max<long>(1, duration_ms / std::max<long>(1, task1.period_ms));
  task2.iterations_to_run =
      std::max<long>(1, duration_ms / std::max<long>(1, task2.period_ms));

  if (use_dds) {
    if (!dds_ctx || !dds_ctx->writer || !dds_ctx->reader) {
      std::cerr << "DDS non inizializzato\n";
      return 1;
    }
    task1.use_dds = true;
    task2.use_dds = true;
    task1.is_publisher = true;
    task2.is_publisher = false;
    task1.dds_writer = dds_ctx->writer;
    task1.dds_reader = dds_ctx->reader;
    task2.dds_writer = dds_ctx->writer;
    task2.dds_reader = dds_ctx->reader;
  }

  pthread_t thread1, thread2;
  pthread_attr_t attr1, attr2;
  pthread_attr_init(&attr1);
  pthread_attr_init(&attr2);
  pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_JOINABLE);
  pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_JOINABLE);

  if (affinity == "SINGLE") {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset);
    pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset);
  } else if (affinity == "MULTI") {
    cpu_set_t cpuset1;
    cpu_set_t cpuset2;
    CPU_ZERO(&cpuset1);
    CPU_ZERO(&cpuset2);
    CPU_SET(0, &cpuset1);
    CPU_SET(2, &cpuset2);
    pthread_attr_setaffinity_np(&attr1, sizeof(cpu_set_t), &cpuset1);
    pthread_attr_setaffinity_np(&attr2, sizeof(cpu_set_t), &cpuset2);
  }

  int ret1 = pthread_create(&thread1, &attr1, PeriodicTask, &task1);
  handle_error(ret1, "Errore creazione PeriodicTask 1");

  int ret2 = pthread_create(&thread2, &attr2, PeriodicTask, &task2);
  handle_error(ret2, "Errore creazione PeriodicTask 2");

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  pthread_attr_destroy(&attr1);
  pthread_attr_destroy(&attr2);

  std::string csv_name =
      "results_" + label + "_" + policy + "_" + affinity + ".csv";
  std::vector<TaskConfig> tasks = {task1, task2};
  ExportResultsToCSV(csv_name, label + "_" + policy + "_" + affinity, tasks,
                     config_xml);

  return 0;
}

int RunTestMem(const std::string &policy, const std::string &affinity,
               const std::string &config_xml) {
  return RunTestVariant("mem", policy, affinity, config_xml, false, nullptr);
}

int RunTestDDS(const std::string &policy, const std::string &affinity,
               const std::string &config_xml) {
  static DDSContext dds_ctx;
  static bool dds_ready = false;
  if (!dds_ready) {
    std::string participant_name =
        std::string("RT_Task_Participant_") + policy + "_" + affinity;
    dds_ready = InitDDS(dds_ctx, participant_name);
  }

  return RunTestVariant("dds", policy, affinity, config_xml, true,
                        dds_ready ? &dds_ctx : nullptr);
}
