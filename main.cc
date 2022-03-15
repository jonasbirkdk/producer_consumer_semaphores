/******************************************************************
 * The Main program with the two functions. A simple
 * example of creating and using a thread is provided.
 ******************************************************************/
#include "helper.h"

void* producer(void* id);
struct ProducerStruct {
  int producerID;
  int* jobsArrayPtr;
  int semArrayID;
  int queueSize;
  int jobsPerProducer;
  int* nextJobToProduce;
  ProducerStruct();
  void initialiseProducerStruct(int producerID, int* jobsArray, int semArrayID,
      int queueSize, int jobsPerProducer, int* nextJobToProduce);
};

void* consumer(void* id);
struct ConsumerStruct {
  int consumerID;
  int* jobsArrayPtr;
  int semArrayID;
  int queueSize;
  int* nextJobToConsume;
  ConsumerStruct();
  void initialiseConsumerStruct(int consumerID, int* jobsArrayPtr,
      int semArrayID, int queueSize, int* nextJobToConsume);
};

int main(int argc, char** argv)
{
  // Checking number of command line arguments is correct
  if (argc != 5) {
    fprintf(stderr, "Invalid number of command line arguments\n");
    exit(1);
  }

  // Checking all command line arguments are valid numbers
  for (auto i = 1; i < argc; ++i) {
    if (check_arg(argv[i]) < 0) {
      fprintf(stderr, "Command line argument %d is invalid\n", i);
      exit(1);
    }
  }

  // Initialise input
  auto queueSize = check_arg(argv[1]);
  auto jobsPerProducer = check_arg(argv[2]);
  auto numberOfProducers = check_arg(argv[3]);
  auto numberOfConsumers = check_arg(argv[4]);
  auto firstJobToProduce = 0;
  auto* nextJobToProduce = &firstJobToProduce;
  auto firstJobToConsume = 0;
  auto* nextJobToConsume = &firstJobToConsume;
  auto* jobsArrayPtr = new int[queueSize];

  // Disable buffer for printf()
  setbuf(stdout, NULL);

  // Create semaphores
  auto semArrayID = sem_create(SEM_KEY, SEM_NUM);
  if (semArrayID < 0) {
    perror("Error creating new semaphore array with sem_create()");
    exit(1);
  }
  if (sem_init(semArrayID, QUEUE_MUTEX_SEM, 1) < 0) {
    perror("Error initialising semaphore with sem_init()");
    exit(1);
  }
  if (sem_init(semArrayID, SPACE_SEM, queueSize) < 0) {
    perror("Error initialising semaphore with sem_init()");
    exit(1);
  }
  if (sem_init(semArrayID, JOBS_SEM, 0) < 0) {
    perror("Error initialising semaphore with sem_init()");
    exit(1);
  }

  // Create producer threads
  auto* producerIDs = new pthread_t[numberOfProducers];
  auto* producerInputArray = new ProducerStruct[numberOfProducers];
  for (auto i = 0; i < numberOfProducers; ++i) {
    producerInputArray[i].initialiseProducerStruct(i + 1, jobsArrayPtr,
        semArrayID, queueSize, jobsPerProducer, nextJobToProduce);
  }
  for (auto i = 0; i < numberOfProducers; ++i) {
    pthread_create(
        &producerIDs[i], NULL, producer, (void*)&producerInputArray[i]);
  }

  // Create consumer threads
  auto* consumerIDs = new pthread_t[numberOfConsumers];
  auto* consumerInputArray = new ConsumerStruct[numberOfConsumers];
  for (auto i = 0; i < numberOfConsumers; ++i) {
    consumerInputArray[i].initialiseConsumerStruct(
        i + 1, jobsArrayPtr, semArrayID, queueSize, nextJobToConsume);
  }
  for (auto i = 0; i < numberOfConsumers; ++i) {
    pthread_create(
        &consumerIDs[i], NULL, consumer, (void*)&consumerInputArray[i]);
  }

  // Waiting for all producer and consumer threads to finish executing
  // before exiting main
  for (auto i = 0; i < numberOfProducers; ++i) {
    pthread_join(producerIDs[i], NULL);
  }
  for (auto i = 0; i < numberOfConsumers; ++i) {
    pthread_join(consumerIDs[i], NULL);
  }
  if (sem_close(semArrayID) == -1) {
    perror("Error closing semaphore array with sem_close()");
    exit(1);
  }

  // Cleaning up heap allocated memory
  delete[] jobsArrayPtr;
  delete[] producerIDs;
  delete[] consumerIDs;
  delete[] consumerInputArray;
  delete[] producerInputArray;

  return 0;
}

void* producer(void* parameter)
{
  auto producer = (ProducerStruct*)parameter;
  auto jobsCreated = 0;
  auto jobIdToOutput = 0;
  auto productionDuration = 0;
  auto consumptionDuration = 0;

  while (jobsCreated < producer->jobsPerProducer) {
    // Generate random numbers for how long the producer takes to produce job
    // (1-5 secs) and the consumption duration of the job produced (1-10) secs
    srand(time(0) + producer->producerID);
    productionDuration = (rand() % 5) + 1;
    srand(time(0) + producer->producerID);
    consumptionDuration = (rand() % 10) + 1;

    // Sleep for 1-5 secs to emulate production time
    sleep(productionDuration);

    // Down() SPACE_SEM to check if queue is full and
    // indicate a job is being produced if a space is
    // free within 20 seconds. Exit if no space
    // frees up within 20 seconds
    if (sem_wait_twenty_secs(producer->semArrayID, SPACE_SEM) < 0) {
      fprintf(stdout,
          "Producer(%d): Quit as queue was full for more than 20 seconds.\n",
          producer->producerID);
      pthread_exit(0);
    }

    // Critical section begins: Down() QUEUE_MUTEX_SEM to add new job to queue
    sem_wait(producer->semArrayID, QUEUE_MUTEX_SEM);

    // Assigning the consumption duration to the next vacant spot in the queue
    *(producer->jobsArrayPtr + *producer->nextJobToProduce)
        = consumptionDuration;
    jobIdToOutput = *producer->nextJobToProduce;
    *producer->nextJobToProduce
        = (*producer->nextJobToProduce + 1) % producer->queueSize;

    // Critical section ends: Up() QUEUE_MUTEX_SEM
    sem_signal(producer->semArrayID, QUEUE_MUTEX_SEM);

    // Up JOBS_SEM to indicate one more job is available for consumption
    sem_signal(producer->semArrayID, JOBS_SEM);

    ++jobsCreated;

    fprintf(stdout, "Producer(%d): Job id %d duration %d.\n",
        producer->producerID, jobIdToOutput, consumptionDuration);
  }

  fprintf(stdout, "Producer(%d): No more jobs to generate.\n",
      producer->producerID);

  pthread_exit(0);
}

void* consumer(void* id)
{
  auto consumer = (ConsumerStruct*)id;
  auto jobIdToOutput = 0;
  auto jobDuration = 0;

  while (true) {
    // Down() JOBS_SEM to check if there are any jobs in the queue
    // and reduce the number of jobs in the queue by 1 if
    // one is available for consumption within 20 seconds.
    // Exit if no job becomes available for consumption within 20 seconds
    if (sem_wait_twenty_secs(consumer->semArrayID, JOBS_SEM) < 0) {
      fprintf(
          stdout, "Consumer(%d): No more jobs left.\n", consumer->consumerID);
      pthread_exit(0);
    }

    // Critical section begins: Down() QUEUE_MUTEX_SEM to consume a job
    sem_wait(consumer->semArrayID, QUEUE_MUTEX_SEM);

    jobDuration = *(consumer->jobsArrayPtr + *consumer->nextJobToConsume);
    *(consumer->jobsArrayPtr + *consumer->nextJobToConsume) = 0;
    jobIdToOutput = *consumer->nextJobToConsume;
    *consumer->nextJobToConsume
        = (*consumer->nextJobToConsume + 1) % consumer->queueSize;

    // Critical section ends: Up() QUEUE_MUTEX_SEM
    sem_signal(consumer->semArrayID, QUEUE_MUTEX_SEM);

    // Up SPACE_SEM to indicate one SPACE has freed up in the queue
    // for producers to put a job
    sem_signal(consumer->semArrayID, SPACE_SEM);

    fprintf(stdout, "Consumer(%d): Job id %d executing sleep duration %d.\n",
        consumer->consumerID, jobIdToOutput, jobDuration);

    // Sleep for the duration specified in the job consumed
    sleep(jobDuration);

    fprintf(stdout, "Consumer(%d): Job id %d completed.\n",
        consumer->consumerID, jobIdToOutput);
  }

  pthread_exit(0);
}

/******************************************************************
 * Student's implementation of ProducerStruct and ConsumerStruct
 ******************************************************************/

ProducerStruct::ProducerStruct() {};
void ProducerStruct::initialiseProducerStruct(int producerID, int* jobsArrayPtr,
    int semArrayID, int queueSize, int jobsPerProducer, int* nextJobToProduce)
{
  this->producerID = producerID;
  this->jobsArrayPtr = jobsArrayPtr;
  this->semArrayID = semArrayID;
  this->queueSize = queueSize;
  this->jobsPerProducer = jobsPerProducer;
  this->nextJobToProduce = nextJobToProduce;
};

ConsumerStruct::ConsumerStruct() {};
void ConsumerStruct::initialiseConsumerStruct(int consumerID, int* jobsArrayPtr,
    int semArrayID, int queueSize, int* nextJobToConsume)
{
  this->consumerID = consumerID;
  this->jobsArrayPtr = jobsArrayPtr;
  this->semArrayID = semArrayID;
  this->queueSize = queueSize;
  this->nextJobToConsume = nextJobToConsume;
};