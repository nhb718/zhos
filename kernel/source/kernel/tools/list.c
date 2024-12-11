/**
 * 手写操作系统
 *
 * 文件名: list.c
 * 功  能: 简单的链表操作方法
 */

#include "tools/list.h"


/**
 * 初始化链表
 * @param list 待初始化的链表
 */
void list_init(list_t * list)
{
    list->first = list->last = (list_node_t *)0;
    list->count = 0;
}

/**
 * 将指定表项插入到指定链表的头部
 * @param list 待插入的链表
 * @param node 待插入的结点
 * 两种情况
 * 1) 链表为空
 * 2) 链表不为空
 */
void list_insert_first(list_t * list, list_node_t * node)
{
    // 设置好待插入结点的前后，前面为空
    // 插入链表头, 首先将当前待插入节点node的next指向之前的第一个节点first, 而pre指向0
    node->next = list->first;
    node->pre = (list_node_t *)0;

    // 情况一: 如果链表为空， 需要同时设置first和last指向自己
    if (list_is_empty(list))
    {
        list->last = list->first = node;
    }
    // 情况二: 链表不为空, 将之前第一个first节点的pre指向当前待插入节点node, 而first节点替换成node
    else
    {
        // 否则，设置好原本第一个结点的pre
        list->first->pre = node;
        // 调整first指向
        list->first = node;
    }

    list->count++;
}


/**
 * 将指定表项插入到指定链表的尾部
 * @param list 操作的链表
 * @param node 待插入的结点
 * 两种情况
 * 1) 链表为空
 * 2) 链表不为空
 */
void list_insert_last(list_t * list, list_node_t * node)
{
    // 设置好结点本身
    // 插入链表尾, 首先将当前待插入节点node的pre指向之前的最后一个节点last, 而next指向0
    node->pre = list->last;
    node->next = (list_node_t *)0;

    // 情况一: 如果链表为空, 则first/last都指向唯一的node
    if (list_is_empty(list))
    {
        list->first = list->last = node;
    }
    // 情况二: 链表不为空, 将之前最后一个last节点的next指向当前待插入节点node, 而last节点替换成node
    else
    {
        // 否则，调整last结点的向一指向为node
        list->last->next = node;

        // node变成了新的后继结点
        list->last = node;
    }

    list->count++;
}

/**
 * 移除指定链表的头部
 * @param list 操作的链表
 * @return 链表的第一个结点
 */
list_node_t * list_remove_first(list_t * list)
{
    // 表项为空, 返回空
    if (list_is_empty(list))
    {
        return (list_node_t *)0;
    }

    // 取第一个结点
    list_node_t * remove_node = list->first;

    // 将first往表尾移1个，跳过刚才移过的那个
    list->first = remove_node->next;
    // 情况一: 只有一个节点first==0, last也指向0
    if (list->first == (list_node_t *)0)
    {
        // node为最后一个结点
        list->last = (list_node_t *)0;
    }
    // 情况二: 有多个节点, 将待删除节点的next节点的pre置为0
    else
    {
        // 非最后一结点，将后继的前驱清0
        //remove_node->next->pre = (list_node_t *)0;
        list->first->pre = (list_node_t *)0;
    }

    // 调整待删除节点remove_node自己, 将pre和next都置为0
    remove_node->next = remove_node->pre = (list_node_t *)0;

    // 同时调整计数值
    list->count--;
    return remove_node;
}

/**
 * 移除指定链表的中的表项
 * 不检查node是否在结点中
 */
list_node_t * list_remove(list_t * list, list_node_t * remove_node)
{
    // 如果是头，头往前移
    if (remove_node == list->first)
    {
        list->first = remove_node->next;
    }

    // 如果是尾，则尾往回移
    if (remove_node == list->last)
    {
        list->last = remove_node->pre;
    }

    // 如果有前, 则调整前的后继
    if (remove_node->pre)
    {
        remove_node->pre->next = remove_node->next;
    }

    // 如果有后，则调整后往前的
    if (remove_node->next)
    {
        remove_node->next->pre = remove_node->pre;
    }

    // 清空node指向
    remove_node->pre = remove_node->next = (list_node_t *)0;
    --list->count;
    return remove_node;
}
