#include "heap.h"

ckf::Heap::Heap(const std::vector<int> &arr)
    : _arr(arr)
{
    heapify();
}

int ckf::Heap::top()
{
    return _arr.front();
}

void ckf::Heap::pop()
{
    //交换首尾元素，删掉尾，然后从根节点向下调整
    std::swap(_arr.front(),_arr.back());
    _arr.pop_back();
    adjustDown(0);
}

void ckf::Heap::push(int value)
{
    _arr.push_back(value);
    adjustUp(_arr.size() - 1);
}

void ckf::Heap::heapify()
{
    // 从最后一个父节点开始，向下调整
    for (int root = (_arr.size() - 1 - 1) >> 1; root >= 0; root--)
    {
        adjustDown(root);
    }
}

void ckf::Heap::adjustDown(size_t root)
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
        if (leftChild < n && _arr[leftChild] > _arr[largest])
        {
            largest = leftChild;
        }
        if (rightChild < n && _arr[rightChild] > _arr[largest])
        {
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

void ckf::Heap::adjustUp(size_t pos)
{
    while (pos > 0)
    {
        int parent = (pos - 1) >> 1;
        if (parent >= 0 && _arr[pos] > _arr[parent])
        {
            std::swap(_arr[pos], _arr[parent]);
            pos = parent;
        }
        else
        {
            break;
        }
    }
}

void ckf::Heap::show()
{
    for (auto x : _arr)
    {
        std::cout << x << " ";
    }
    std::cout << std::endl;
}
