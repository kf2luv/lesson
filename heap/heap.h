#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>

namespace ckf
{
    class Heap
    {
    public:
        // 大根堆（完全二叉树）
        Heap(const std::vector<int> &arr);

        int top();
        void pop();
        void push(int value);

        void show();

    private:
        // 创建堆
        void heapify();
        // 向下调整
        // 左子树和右子树都是大根堆时，向下调整当前节点，使当前节点为根节点的树调整为大根堆
        void adjustDown(size_t root);
        // 向上调整
        void adjustUp(size_t pos);

    private:
        std::vector<int> _arr;
    };
} // ckf