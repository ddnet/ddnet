#include <gtest/gtest.h>

#include <engine/shared/ringbuffer.h>

TEST(DISABLED_Ringbuffer, Small)
{
    // TODO: stack smashing detected
    TStaticRingBuffer<int, 4> buffer;
}

TEST(DISABLED_Ringbuffer, Bigger)
{
    TStaticRingBuffer<int, 32> buffer;

    buffer.Init();

    int *pMem1 = buffer.Allocate(1024);

    // TODO: segfault
    pMem1 = buffer.Next(pMem1);
}

TEST(Ringbuffer, Big)
{
    TStaticRingBuffer<int, 4096> buffer;

    buffer.Init();

    int *pMem1 = buffer.Allocate(sizeof(int)*4096);
    buffer.Prev(pMem1);
}
