/*_
 * Copyright (c) 2019 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "kernel.h"
#include "tree.h"

/*
 * Add a node to the binary tree
 */
int
btree_add(btree_node_t **t, btree_node_t *n, int (*comp)(void *, void *),
          int allowdup)
{
    int ret;

    if ( NULL == *t ) {
        /* Insert here */
        *t = n;
        n->left = NULL;
        n->right = NULL;
        return 0;
    }
    ret = comp(n, *t);
    if ( !allowdup && 0 == ret ) {
        return -1;
    }
    if ( ret > 0 ) {
        return btree_add(&(*t)->right, n, comp, allowdup);
    } else {
        return btree_add(&(*t)->left, n, comp, allowdup);
    }
    return 0;
}

/*
 * Remove a node from the binary tree
 */
void *
btree_delete(btree_node_t **t, btree_node_t *n, int (*comp)(void *, void *))
{
    btree_node_t **x;

    if ( NULL == *t ) {
        /* Not found */
        return NULL;
    }
    if ( n == *t ) {
        if ( n->left && n->right ) {
            *t = n->left;
            x = &n->left;
            while ( NULL != *x ) {
                x = &(*x)->right;
            }
            *x = n->right;
        } else if ( n->left ) {
            *t = n->left;
        } else if ( n->right ) {
            *t = n->right;
        } else {
            *t = NULL;
        }
        return n;
    }
    if ( comp(n, (*t)) > 0 ) {
        return btree_delete(&(*t)->right, n, comp);
    } else {
        return btree_delete(&(*t)->left, n, comp);
    }
}

/*
 * Search
 */
btree_node_t *
btree_search(btree_node_t *n, int (*comp)(void *))
{
    int ret;

    ret = comp(n);
    if ( 0 == ret ) {
        return n->data;
    } else if ( ret > 0 ) {
        return btree_search(n->right, comp);
    } else {
        return btree_search(n->left, comp);
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
