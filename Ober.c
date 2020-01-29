#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>

int number_of_cabs , number_of_riders , number_of_payment_servers,number_of_riders_done;

typedef struct Cab{
	int idx;
	int status;
    pthread_mutex_t mutex;
}Cab;

typedef struct Rider{
	int idx;
	int status;
	int wait_time;
	int ride_time;
	int cab_type;
	pthread_t rider_thread_id;
	int cab_id;
	int server_id;
}Rider;

typedef struct Server{
	int idx;
	int status;
	int rider_id;
    pthread_mutex_t mutex;
	pthread_t server_thread_id;
}Server;


void * BookCab(void * args);
void * WaitForRideEnd(void *args);
void * AcceptPayment(void *args);
void * MakePayment(void *args);

Cab **cab;
Rider **rider;
Server **server;

sem_t semcabs;
sem_t semservers;
sem_t sempoolcabs;

pthread_mutex_t mutexcnt;


void * AcceptPayment(void *args)
{
	Server *server = (Server*)args;
	while(number_of_riders_done < number_of_riders)
	{	
		if(server->status == 1)
		{
			sleep(2);
			printf("Rider %d had done the payment at payment server %d\n",server->rider_id+1,server->idx+1);
			fflush(stdout);
			server->status = 0;
			sem_post(&semservers);
			pthread_mutex_t * mutex_point1 = &(mutexcnt);
            pthread_mutex_lock(mutex_point1);
            number_of_riders_done++;
            pthread_mutex_unlock(mutex_point1);

		}
	}	
}

void * MakePayment(void *args)
{
	Rider *rider = (Rider*)args;
	int s = sem_wait(&semservers);
	for(int i=0;i<number_of_payment_servers;i++)
	{
		pthread_mutex_t * mutex_point = &(server[i]->mutex);
        pthread_mutex_lock(mutex_point);

        if(server[i]->status == 0)
        {
        	server[i]->status = 1;
        	rider->server_id = server[i]->idx;
        	rider->status = 2;
        	server[i]->rider_id = rider->idx;
    		pthread_mutex_unlock(mutex_point);
        	printf("Rider %d got access to payment server %d\n",rider->idx+1,rider->server_id+1);
			fflush(stdout);
        	break;
        }
        else
        {
        	pthread_mutex_unlock(mutex_point);
        }
	}
}

 
void * BookCab(void * args)
{	
	sleep(1+rand()%10);
	Rider *rider = (Rider*)args;
	char str[15];
	if(rider->cab_type == 1)
	{
		strcpy(str,"Premier");
	}
	else if(rider->cab_type == 2)
	{
		strcpy(str,"Pool");
	}
	printf("Rider %d demands cab_type : %s\n",rider->idx+1,str);
	fflush(stdout);
	if(rider->cab_type == 1)
	{
		struct timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
		{
		    return NULL;
		}

		ts.tv_sec += rider->wait_time;;
		int s;
		s = sem_timedwait(&semcabs, &ts);
		   
   		if (s == -1)
		{
		    if (errno == ETIMEDOUT)
		    {
		        printf("rider %d exited due excess waiting\n",rider->idx+1);
				fflush(stdout);
				pthread_mutex_t * mutex_point1 = &(mutexcnt);
            	pthread_mutex_lock(mutex_point1);
            	number_of_riders_done++;
            	pthread_mutex_unlock(mutex_point1);
			}	
		    else
		        perror("sem_timedwait");
		} 
		else
		{
			for(int i=0;i<number_of_cabs;i++)
			{
				pthread_mutex_t * mutex_point = &(cab[i]->mutex);
                pthread_mutex_lock(mutex_point);
                if(cab[i]->status == 0)
                {
                	cab[i]->status = 1;
                	rider->status = 1;
                	rider->cab_id = cab[i]->idx;
		            printf("Rider %d got the ride in cab %d\n",rider->idx+1,rider->cab_id+1);
					fflush(stdout);
		            pthread_mutex_unlock(mutex_point);
                	// WaitForRideEnd(rider);
                	sleep(rider->ride_time);

					pthread_mutex_t * mutex_point = &(cab[rider->cab_id]->mutex);
				    pthread_mutex_lock(mutex_point);
					
				    printf("Rider %d has finished its ride from cab %d\n",rider->idx+1,rider->cab_id+1);
				    fflush(stdout);
					while(1)
					{	
						if(cab[rider->cab_id]->status == 3)
						{
							cab[rider->cab_id]->status = 2;
							sem_post(&sempoolcabs);
				    		pthread_mutex_unlock(mutex_point);
							MakePayment(rider);
	                		break;
						}
						else if(cab[rider->cab_id]->status == 1)
						{
							cab[rider->cab_id]->status = 0;
							sem_post(&semcabs);
				    		pthread_mutex_unlock(mutex_point);
							MakePayment(rider);
	                		break;
						}
						else if(sem_trywait(&sempoolcabs) == 0)
						{
							cab[rider->cab_id]->status = 0;
							sem_post(&semcabs);
				    		pthread_mutex_unlock(mutex_point);
							MakePayment(rider);
	                		break;
						}
					    
	                }	
            		break;
                }
			
				else
				{
		            pthread_mutex_unlock(mutex_point);
				}
			}	
		}	
	}
	else if(rider->cab_type == 2)
	{
		int value = sem_trywait(&sempoolcabs);
		

		if(value == -1)
		{	
       
			struct timespec ts;
			if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
			{
			    return NULL;
			}

			ts.tv_sec += rider->wait_time;
			int s;
			s = sem_timedwait(&semcabs, &ts);     
	   		if (s == -1)
			{
			    if (errno == ETIMEDOUT)
			    {	
			        printf("rider %d exited due excess waiting\n",rider->idx+1);
			    	fflush(stdout);
			    	pthread_mutex_t * mutex_point1 = &(mutexcnt);
		            pthread_mutex_lock(mutex_point1);
		            number_of_riders_done++;
		            pthread_mutex_unlock(mutex_point1);
			    }
			    else
			        perror("sem_timedwait");
			} 
			else
			{
				for(int i=0;i<number_of_cabs;i++)
				{
					pthread_mutex_t * mutex_point = &(cab[i]->mutex);
	                pthread_mutex_lock(mutex_point);
	                if(cab[i]->status == 0)
	                {
	                	cab[i]->status = 2;
	                	rider->status = 1;
	                	rider->cab_id = cab[i]->idx;
	                	sem_post(&sempoolcabs);
			            printf("Rider %d got the ride in cab %d\n",rider->idx+1,rider->cab_id+1);
						fflush(stdout);
			            pthread_mutex_unlock(mutex_point);
	                	sleep(rider->ride_time);
						pthread_mutex_t * mutex_point = &(cab[rider->cab_id]->mutex);
					    pthread_mutex_lock(mutex_point);
						
					    printf("Rider %d has finished its ride from cab %d\n",rider->idx+1,rider->cab_id+1);
					    fflush(stdout);
						while(1)
						{	
							if(cab[rider->cab_id]->status == 3)
							{
								cab[rider->cab_id]->status = 2;
								sem_post(&sempoolcabs);
				    			pthread_mutex_unlock(mutex_point);
								MakePayment(rider);
		                		break;
							}
							else if(cab[rider->cab_id]->status == 1)
							{
								cab[rider->cab_id]->status = 0;
								sem_post(&semcabs);
				    			pthread_mutex_unlock(mutex_point);
								MakePayment(rider);
		                		break;
							}
							else if(sem_trywait(&sempoolcabs) == 0)
							{
								cab[rider->cab_id]->status = 0;
								sem_post(&semcabs);
				    			pthread_mutex_unlock(mutex_point);
								MakePayment(rider);
		                		break;
							}
						    
		                }
		                break;	
	                }
					else
					{
		            	pthread_mutex_unlock(mutex_point);
					}
				}	
			}	
		}
		else if(value == 0)
		{
			for(int i=0;i<number_of_cabs;i++)
			{
				pthread_mutex_t * mutex_point = &(cab[i]->mutex);
                pthread_mutex_lock(mutex_point);
                if(cab[i]->status == 2)
                {
                	cab[i]->status = 3;
                	rider->status = 1;
                	rider->cab_id = cab[i]->idx;
		            printf("Rider %d got the ride in cab %d\n",rider->idx+1,rider->cab_id+1);
					fflush(stdout);
		            pthread_mutex_unlock(mutex_point);

					sleep(rider->ride_time);
					pthread_mutex_t * mutex_point = &(cab[rider->cab_id]->mutex);
				    pthread_mutex_lock(mutex_point);
				    printf("Rider %d has finished its ride from cab %d\n",rider->idx+1,rider->cab_id+1);
				    fflush(stdout);
					while(1)
					{	
						if(cab[rider->cab_id]->status == 3)
						{
							cab[rider->cab_id]->status = 2;
							sem_post(&sempoolcabs);
				    		pthread_mutex_unlock(mutex_point);
							MakePayment(rider);
	                		break;
						}
						else if(cab[rider->cab_id]->status == 1)
						{
							cab[rider->cab_id]->status = 0;
							sem_post(&semcabs);
				    		pthread_mutex_unlock(mutex_point);
							MakePayment(rider);
	                		break;
						}
						else if(sem_trywait(&sempoolcabs) == 0)
						{
							cab[rider->cab_id]->status = 0;
							sem_post(&semcabs);
				    		pthread_mutex_unlock(mutex_point);
							MakePayment(rider);
	                		break;
						}
					    
	                }
	                break;	
                }
			
				else
				{
		            pthread_mutex_unlock(mutex_point);
				}
			}

		}
	}

}


int main()
{	
	srand(time(0));
	int i,j;
	printf("Please enter the number of Cabs\n");
    scanf("%d",&number_of_cabs);

    printf("Please enter the number of Payment servers\n");
    scanf("%d",&number_of_payment_servers);

    printf("Please enter the number of Riders\n");
    scanf("%d",&number_of_riders);

    if(!number_of_riders || !number_of_cabs || !number_of_payment_servers)
    {
    	printf("All the inputs should neccessarily be greater than zero\n");
    	return 0;
    }

    number_of_riders_done = 0;

    sem_init(&(semcabs),0,number_of_cabs);
    sem_init(&(sempoolcabs),0,0);
    sem_init((&semservers),0,number_of_payment_servers);
    pthread_mutex_init(&(mutexcnt), NULL);


    cab = (Cab**)malloc((sizeof(Cab*))*number_of_cabs);

    for(i=0;i<number_of_cabs;i++)
    {
    	cab[i] = (Cab*)malloc(sizeof(Cab));
        pthread_mutex_init(&(cab[i]->mutex), NULL);
        cab[i]->idx = i;
        cab[i]->status = 0;
    }

    server = (Server**)malloc((sizeof(Server*))*number_of_payment_servers);

    for(i=0;i<number_of_payment_servers;i++)
    {
    	server[i] = (Server*)malloc(sizeof(Server));
    	server[i]->idx = i;
        pthread_mutex_init(&(server[i]->mutex), NULL);
		server[i]->status  = 0;
    }

    rider = (Rider**)malloc((sizeof(Rider*))*number_of_riders);

    for(i=0;i<number_of_riders;i++)
    {
    	rider[i] = (Rider*)malloc(sizeof(Rider));
    	rider[i]->idx = i;
		rider[i]->status = 0;
		rider[i]->wait_time = 4+rand()%4;
		rider[i]->ride_time = 2+rand()%3;
		rider[i]->cab_type = 1 + rand()%2;
		rider[i]->cab_id=-1;
    }

    for(i=0;i<number_of_riders;i++)
    {
    	pthread_create(&(rider[i]->rider_thread_id),NULL,BookCab,rider[i]);
    }

    for(i=0;i<number_of_payment_servers;i++)
    {
    	pthread_create(&(server[i]->server_thread_id),NULL,AcceptPayment,server[i]);	
    }

    for(i=0;i<number_of_riders;i++)
    {
    	pthread_join(rider[i]->rider_thread_id,0);
    }

    for(i=0;i<number_of_payment_servers;i++)
    {
    	pthread_join(server[i]->server_thread_id,0);
    }

    printf("Simulation Done\n");
    return 0;
}