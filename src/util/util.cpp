#include "ltype.h"
#include "comf.h"
#include "smempool.h"
#include "sstl.h"
#include "errno.h"
#include "util.h"

#define ERR_BUF_LEN 1024

INT g_indent = 0;
CHAR * g_indent_chars = (CHAR*)" ";
SMEM_POOL * g_pool_tmp_used = NULL;
FILE * g_tfile = NULL;

void interwarn(CHAR const* format, ...)
{
	CHAR sbuf[ERR_BUF_LEN];
	if (strlen(format) > ERR_BUF_LEN) {
		IS_TRUE(0, ("internwarn message is too long to print"));
	}
	//CHAR * arg = (CHAR*)((CHAR*)(&format) + sizeof(CHAR*));
	va_list arg;
	va_start(arg, format);
	vsprintf(sbuf, format, arg);
	printf("\n!!!XOC INTERNAL WARNING:%s\n\n", sbuf);
	va_end(arg);
}


//Print string.
INT prt(CHAR const* format, ...)
{
	if (format == NULL) return 0;
	CHAR buf[MAX_BUF_LEN];
	if (strlen(format) > MAX_BUF_LEN) {
		IS_TRUE(0, ("prt message is too long to print"));
	}
	//CHAR * arg = (CHAR*)((CHAR*)(&format) + sizeof(CHAR*));
	va_list arg;
	va_start(arg, format);
	vsprintf(buf, format, arg);
    if (g_tfile != NULL) {
		fprintf(g_tfile, "%s", buf);
		fflush(g_tfile);
	} else {
		fprintf(stdout, "%s", buf);
	}
	va_end(arg);
	return 0;
}


//NOTE: message should not exceed MAX_BUF_LEN.
void scr(CHAR const* format, ...)
{
	UNUSED(format);
#ifdef _DEBUG_
	//CHAR * arg = (CHAR*)((CHAR*)(&format) + sizeof(CHAR*));
	va_list arg;
	va_start(arg, format);
	CHAR buf[MAX_BUF_LEN];
	vsprintf(buf, format, arg);
	fprintf(stdout, "\n%s", buf);
	va_end(arg);
#endif
}


void finidump()
{
	if (g_tfile != NULL) {
		fclose(g_tfile);
		g_tfile = NULL;
	}
}


void initdump(CHAR const* f, bool is_del)
{
	if (is_del && f != NULL) {
		unlink(f);
	}
	if (f != NULL) {
		if (is_del) {
			unlink(f);
		}
		g_tfile = fopen(f, "a+");
		if (g_tfile == NULL) {
			IS_TRUE(0, ("can not open %s, errno:%d, errstring is %s",
						f, errno, strerror(errno)));
			return;
		}
	} else {
		//g_tfile = stdout;
	}
}


//Print string with indent chars.
void note(CHAR const* format, ...)
{
	if (g_tfile == NULL) {
		return;
	}
	if (format == NULL) return;
	CHAR buf[MAX_BUF_LEN];
	CHAR * real_buf = buf;
	//CHAR * arg = (CHAR*)((CHAR*)(&format) + sizeof(CHAR*));
	va_list arg;
	va_start(arg, format);
	vsnprintf(buf, MAX_BUF_LEN, format, arg);
	buf[MAX_BUF_LEN - 1] = 0;
	UINT len = strlen(buf);
	IS_TRUE0(len < MAX_BUF_LEN);
	UINT i = 0;
	while (i < len) {
		if (real_buf[i] == '\n') {
			fprintf(g_tfile, "\n");
		} else {
			break;
		}
		i++;
	}

	//Append indent chars ahead of string.
	INT w = 0;
	while (w < g_indent) {
		fprintf(g_tfile, "%s", g_indent_chars);
		w++;
	}

	if (i == len) {
		fflush(g_tfile);
		goto FIN;
	}

	fprintf(g_tfile, "%s", real_buf + i);
	fflush(g_tfile);
FIN:
	va_end(arg);
	return;
}


//Malloc memory for tmp used.
void * tlloc(LONG size)
{
	if (size < 0 || size == 0) return NULL;
	if (g_pool_tmp_used == NULL) {
		g_pool_tmp_used = smpool_create_handle(8, MEM_COMM);
	}
	void * p = smpool_malloc_h(size, g_pool_tmp_used);
	if (p == NULL) return NULL;
	memset(p, 0, size);
	return p;
}


void tfree()
{
	if (g_pool_tmp_used != NULL) {
		smpool_free_handle(g_pool_tmp_used);
		g_pool_tmp_used = NULL;
	}
}


void dump_vec(SVECTOR<UINT> & v)
{
	if (g_tfile == NULL) return;
	fprintf(g_tfile, "\n");
	for (INT i = 0; i <= v.get_last_idx(); i++) {
		UINT x = v.get(i);
		if (x == 0) {
			fprintf(g_tfile, "0,");
		} else {
			fprintf(g_tfile, "0x%x,", x);
		}
	}
	fflush(g_tfile);
}


//Integer TAB.
class IT : public RBT<int, int> {
public:
	//Add left child
	void add_lc(int from, RBCOL f, int to, RBCOL t)
	{
		TN * x = m_root;
		if (x == NULL) {
			m_root = new_tn(from, f);
			x = new_tn(to, t);
			m_root->lchild = x;
			x->parent = m_root;
			return;
		}

		LIST<TN*> lst;
		lst.append_tail(x);
		while (lst.get_elem_count() != 0) {
			x = lst.remove_head();
			if (x->key == from) {
				break;
			}
			if (x->rchild != NULL) {
				lst.append_tail(x->rchild);
			}
			if (x->lchild != NULL) {
				lst.append_tail(x->lchild);
			}
		}
		IS_TRUE0(x);
		IS_TRUE0(x->color == f);
		TN * y = new_tn(to, t);

		IS_TRUE0(x->lchild == NULL);
		x->lchild = y;
		y->parent = x;
	}

	//Add left child
	void add_rc(int from, RBCOL f, int to, RBCOL t)
	{
		TN * x = m_root;
		if (x == NULL) {
			m_root = new_tn(from, f);
			x = new_tn(to, t);
			m_root->rchild = x;
			x->parent = m_root;
			return;
		}

		LIST<TN*> lst;
		lst.append_tail(x);
		while (lst.get_elem_count() != 0) {
			x = lst.remove_head();
			if (x->key == from) {
				break;
			}
			if (x->rchild != NULL) {
				lst.append_tail(x->rchild);
			}
			if (x->lchild != NULL) {
				lst.append_tail(x->lchild);
			}
		}
		IS_TRUE0(x);
		IS_TRUE0(x->color == f);
		TN * y = new_tn(to, t);

		IS_TRUE0(x->rchild == NULL);
		x->rchild = y;
		y->parent = x;
	}
};


static void test1()
{
	IT x;
	x.add_lc(11, RBBLACK, 2, RBRED);
	x.add_rc(11, RBBLACK, 14, RBBLACK);

	x.add_lc(2, RBRED, 1, RBBLACK);
	x.add_rc(2, RBRED, 7, RBBLACK);

	x.add_lc(7, RBBLACK, 5, RBRED);
	x.add_rc(7, RBBLACK, 8, RBRED);

	x.add_rc(14, RBBLACK, 15, RBRED);

	//x.add_lc(5, RBRED, 4, RBRED);
	x.insert(4);
	dump_rbt(x);
}


static void test2()
{
	IT x;
	x.insert(1);
	x.insert(2);
	x.insert(11);
	x.insert(14);
	x.insert(15);
	x.insert(7);
	x.insert(5);
	x.insert(8);
	x.insert(4);
	x.insert(4);
	dump_rbt(x);

	//Test remove
	x.remove(7);
	x.remove(8);
	x.remove(4);
	x.remove(11);
	x.remove(14);
	x.remove(2);
	x.remove(5);
	x.remove(15);
	x.remove(1);

	//Test insert
	x.insert(1);
	x.insert(2);
	x.insert(11);
	x.insert(14);
	x.insert(15);
	x.insert(7);
	x.insert(5);
	x.insert(8);
	x.insert(4);
	x.insert(4);
	dump_rbt(x);
}


void test_rbt()
{
	test1();
	test2();
}
