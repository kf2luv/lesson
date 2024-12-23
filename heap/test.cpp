#include "heap.h"

void testCreateHeap()
{
    ckf::Heap h1({1, 8, 3, 9, 4, 5, 6});
    h1.show();

    ckf::Heap h2({8, 94, 25, 59, 5, 57, 14, 34, 53, 61});
    h2.show();
}

void testPushPop(){
    ckf::Heap h1({1, 8, 3, 9, 4, 5, 6});
    h1.show();

    h1.push(7);
    h1.push(10);
    h1.show();

    h1.pop();
    h1.show();
}

int main()
{
    // testCreateHeap();
    testPushPop();
}