#ifndef QUEUE_HEADER_CUSTOM
#define QUEUE_HEADER_CUSTOM

#include <stdio.h>

#define SIZE 1000
struct BarrierEvent
{
    int timestamp;
    int barrierID;
    bool on;
};
struct BarrierEvent inp_arr[SIZE];
void enqueue(struct BarrierEvent);
struct BarrierEvent dequeue();
struct BarrierEvent top();
void show();

int Rear = -1;
int Front = -1;

void enqueue(struct BarrierEvent barrierEvent)
{
    printf("Here in enqueue \n");
    if (Rear == SIZE - 1)
        printf("Overflow \n");
    else
    {
        if (Front == -1)
            Front = 0;
        Rear = Rear + 1;
        inp_arr[Rear] = barrierEvent;
    }
}

struct BarrierEvent dequeue()
{
    if (Front == -1 || Front > Rear)
    {
        printf("Underflow \n");
        return;
    }
    else
    {
        struct BarrierEvent item = inp_arr[Front];
        Front = Front + 1;
        return item;
    }
}

struct BarrierEvent top()
{
    return inp_arr[Front];
}

void show()
{
    // if (Front == -1)
    // {
    //     printf("Empty Queue \n");
    // }
    // else
    // {
    //     printf("Queue: \n");
    //     for (int i = Front; i <= Rear; i++)
    //         printf("%d ", inp_arr[i]);
    //     printf("\n");
    // }
}

#endif