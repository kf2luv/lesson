#include "heap.h"

// void testCreateHeap()
// {
//     ckf::Heap h1({1, 8, 3, 9, 4, 5, 6});
//     h1.show();

//     ckf::Heap h2({8, 94, 25, 59, 5, 57, 14, 34, 53, 61});
//     h2.show();
// }

// void testPushPop(){
//     ckf::Heap h1({1, 8, 3, 9, 4, 5, 6});
//     h1.show();

//     h1.push(7);
//     h1.push(10);
//     h1.show();

//     h1.pop();
//     h1.show();
// }

class MyComp1
{
public:
    bool operator()(const int &lhs, const int &rhs)
    {
        return lhs < rhs; // 控制堆中元素大的往上走
    }
};

class MyComp2
{
public:
    bool operator()(const int &lhs, const int &rhs)
    {
        return lhs > rhs; // 控制堆中元素小的往上走
    }
};

//符合条件时（返回值为true），比较器comp的第二个参数往上走

void testTemplateHeap()
{
    ckf::Heap<int, MyComp2> heap({1, 8, 3, 9, 4, 5, 6});
    heap.show();
}

int main()
{
    // testCreateHeap();
    // testPushPop();
    testTemplateHeap();
}