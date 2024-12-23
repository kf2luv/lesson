#pragma once
#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>

namespace ckf
{
    // 模板指定类型和比较器
    template <class T, class Comparison>
    class Heap
    {
    public:
        Heap(const std::vector<T> &arr);
        Heap();
        T top();
        size_t size();
        bool empty();
        void pop();
        void push(const T &value);

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
        std::vector<T> _arr;
        Comparison _comp; // 比较器
    };
} // ckf

template <class T, class Comparison>
ckf::Heap<T, Comparison>::Heap(const std::vector<T> &arr)
    : _arr(arr)
{
    //将传入的序列堆化
    heapify();
}

template <class T, class Comparison>
ckf::Heap<T, Comparison>::Heap()
{
}

template <class T, class Comparison>
inline T ckf::Heap<T, Comparison>::top()
{
    assert(!empty());
    return _arr.front();
}

template <class T, class Comparison>
inline size_t ckf::Heap<T, Comparison>::size()
{
    return _arr.size();
}

template <class T, class Comparison>
inline bool ckf::Heap<T, Comparison>::empty()
{
    return size() == 0;
}

template <class T, class Comparison>
void ckf::Heap<T, Comparison>::pop()
{
    assert(!empty());
    // 交换首尾元素，删掉尾，然后从根节点向下调整
    std::swap(_arr.front(), _arr.back());
    _arr.pop_back();
    if (empty())
    { // 只有一个元素，就不用向下调整了
        return;
    }
    adjustDown(0);
}

template <class T, class Comparison>
void ckf::Heap<T, Comparison>::push(const T &value)
{
    _arr.push_back(value);
    adjustUp(_arr.size() - 1);
}

template <class T, class Comparison>
void ckf::Heap<T, Comparison>::heapify()
{
    // 从最后一个父节点开始，向下调整
    for (int root = (_arr.size() - 1 - 1) >> 1; root >= 0; root--)
    {
        adjustDown(root);
    }
}

template <class T, class Comparison>
void ckf::Heap<T, Comparison>::adjustDown(size_t root)
{
    int n = _arr.size();
    assert(root >= 0 && root < n);

    int pos = root;
    int largest = pos;
    while (pos < n)
    {
        int leftChild = (pos << 1) + 1;
        int rightChild = leftChild + 1;

        // 得出root/left/right三者最大的节点largest
        if (leftChild < n && _comp(_arr[largest], _arr[leftChild]))
        {
            // leftChild满足条件，往上走
            largest = leftChild;
        }
        if (rightChild < n && _comp(_arr[largest], _arr[rightChild]))
        {
            // rightChild满足条件，往上走
            largest = rightChild;
        }

        if (largest == pos)
        {
            // 最大值就是当前pos，不用调整了
            return;
        }

        std::swap(_arr[pos], _arr[largest]);
        pos = largest;
    }
}

template <class T, class Comparison>
void ckf::Heap<T, Comparison>::adjustUp(size_t pos)
{
    while (pos > 0)
    {
        int parent = (pos - 1) >> 1;
        if (parent >= 0 && _comp(_arr[parent], _arr[pos]))
        {
            // pos满足条件，往上走
            std::swap(_arr[pos], _arr[parent]);
            pos = parent;
        }
        else
        {
            break;
        }
    }
}

template <class T, class Comparison>
void ckf::Heap<T, Comparison>::show()
{
    for (auto x : _arr)
    {
        std::cout << x << " ";
    }
    std::cout << std::endl;
}
