/*@
Copyright (c) 2013-2014, Su Zhenyu steven.known@gmail.com 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Su Zhenyu nor the names of its contributors
      may be used to endorse or promote products derived from this software 
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
@*/
#ifndef __LPSOLF_H_
#define __LPSOLF_H_
/*
The file includes the following main components:
 * implementation of the revised simplex method.
 * implementation of the exact simplex method based on rational arithmetic.
 * implementation of the approximate simplex method based 
 	on double/float arithmetic.
 * implementation of the 0-1 integer programming solver.
 * implementation of the mixed integer programming solver.
*/


#define IS_INEQ(a, b) ((a) != (b))
#define IS_EQ(a, b) ((a) == (b))
#define IS_GE(a, b) ((a) >= (b))
#define IS_LE(a, b) ((a) <= (b))
#define INVALID_EQNUM -1
#define SIX_DUMP_NAME "zsix.cxx"

class PIVOT_PAIR {
public:
	INT flag;
	INT nvidx; //pivoting candidate, nonbasic variable index.
	INT bvidx; //pivoting candidate, basic variable index.
	PIVOT_PAIR(INT nv, INT bv)
	{
		init(nv, bv);
	}

	void init(INT nv, INT bv)
	{
		IS_TRUE(nv >= 0 && bv >= 0, ("illegal index")); 
		nvidx = nv; 
		bvidx = bv;
		flag = 1;
	}
};


class PIVOT_PAIR_TAB {
public:
	bool m_is_init;
	BMAT m_pair; //Pairs of nv,bv.

	PIVOT_PAIR_TAB(INT varnum)
	{
		m_is_init = false;
		init(varnum);
	}

	~PIVOT_PAIR_TAB()
	{
		destroy();
	}
	
	void init(INT varnum)
	{
		if (m_is_init) return;
		m_pair.reinit(varnum, varnum);
		m_is_init = true;
	}

	void destroy()
	{
		if (!m_is_init) return;
		m_pair.destroy();
		m_is_init = false;
	}

	//Generate a pair of non-basic variable and basic variable.
	//Rows indicate NV, cols indicate BV.
	void gen_pair(INT nvidx, INT bvidx)
	{
		IS_TRUE0(nvidx != bvidx);
		m_pair.set(nvidx, bvidx, true);
	}

	//Rows indicate NV, cols indicate BV.
	bool is_handle(INT nvidx, INT bvidx) const 
	{
		IS_TRUE0(nvidx != bvidx);
		return m_pair.get(nvidx, bvidx);
	}

	//Rows indicate NV, cols indicate BV.
	void disable_nv(UINT idx)
	{
		IS_TRUE0(idx < m_pair.get_row_size());
		for (UINT j = 0; j < m_pair.get_col_size(); j++) {			
			if (j == idx) {
				continue;
			}
			m_pair.set(idx, j, 1);
		} //end for
	}

	//Rows indicate NV, cols indicate BV.
	bool can_be_nv_cand(UINT idx) const
	{
		IS_TRUE0(idx < m_pair.get_row_size());
		for (UINT j = 0; j < m_pair.get_col_size(); j++) {			
			if (j == idx) {
				continue;
			}
			if (!m_pair.get(idx, j)) {
				//At least one chance!
				return true;
			}
		} //end for
		return false;
	}

	//Rows indicate NV, cols indicate BV.
	bool can_be_bv_cand(UINT idx) const
	{
		IS_TRUE0(idx < m_pair.get_col_size());
		for (UINT i = 0; i < m_pair.get_row_size(); i++) {
			if (i == idx) {
				continue;
			}
			if (!m_pair.get(i, idx)) {
				//At least one chance!
				return true;
			}
		} //end for
		return false;
	}
};


template <class T> class ELEMT {
};


template <class MAT> class PV_PARM {
public:
	MAT * eq; 
	MAT * tgtf; 
	SVECTOR<bool> * nvset; 
	SVECTOR<bool> * bvset;
	SVECTOR<INT> * bv2eqmap;
	SVECTOR<INT> * eq2bvmap;
	PIVOT_PAIR_TAB * ppt;
	INT rhs_idx;
	
	PV_PARM(MAT * peq, 
			MAT * ptgtf, 
			SVECTOR<bool> * pnvset, 
			SVECTOR<bool> * pbvset,
			SVECTOR<INT> * pbv2eqmap, 
			SVECTOR<INT> * peq2bvmap, 
			PIVOT_PAIR_TAB * pppt,
			INT prhs_idx)
	{
		eq = peq; 
		tgtf = ptgtf; 
		nvset = pnvset; 
		bvset = pbvset;
		bv2eqmap = pbv2eqmap; 
		eq2bvmap = peq2bvmap; 
		ppt = pppt;
		rhs_idx = prhs_idx;
	}
};



//
//START SIX
//
/*
Definitive Simplex Method.
*/
#define SIX_SUCC					0
#define	SIX_UNBOUND					1
#define SIX_NO_PRI_FEASIBLE_SOL		2
#define SIX_OPTIMAL_IS_INFEASIBLE	3
#define SIX_TIME_OUT				4

template <class MAT, class T> class SIX : public ELEMT<T> {
	bool m_is_init;
	INT m_rhs_idx; 
	UINT m_indent;
	UINT m_max_iter;
	PIVOT_PAIR_TAB * m_ppt;

	INT _stage_1(OUT MAT & newleq, 
				OUT MAT & newvc, 
				OUT MAT & newtgtf,
				OUT SVECTOR<bool> & nvset, 
				OUT SVECTOR<bool> & bvset, 
				SVECTOR<INT> & bv2eqmap, 
				SVECTOR<INT> & eq2bvmap,
				IN OUT INT & new_rhs_idx);
	void _new_ppt(INT rhs_idx);
	void _pivot(IN UINT nv, IN UINT bv, IN OUT PV_PARM<MAT> & pp);
	INT _find_pivot_bv(UINT pivot_nv, IN OUT PV_PARM<MAT> & pp);
	INT _find_pivot_nv_bv_pair(OUT INT & nvidx, OUT INT & bvidx, 
							IN OUT PV_PARM<MAT> & pp);
	UINT _solve_slack_form(IN OUT MAT & tgtf, IN OUT MAT & eqc,
							IN OUT MAT & vc, 
							OUT T & maxv, 
							OUT MAT & sol,
							IN OUT SVECTOR<bool> & nvset, 
							IN OUT SVECTOR<bool> & bvset,
							IN OUT SVECTOR<INT> & bv2eqmap, 
							IN OUT SVECTOR<INT> & eq2bvmap, 
							IN INT rhs_idx);
	void _verify(IN MAT const& leq, 
				IN MAT const& eq, 
				IN MAT const& tgtf, 
				IN MAT const& vc,
				IN INT rhs_idx);
	INT _normalize(OUT MAT & newleq, OUT MAT & newvc, OUT INTMAT & vcmap,
					OUT MAT & newtgtf,
					IN MAT & vc, IN MAT const& eq, 
					IN MAT const& leq, IN MAT const& tgtf);
	bool _const_term_is_feasible(MAT & newleq, INT rhs_idx);
	void _slack(IN OUT MAT & tgtf, IN OUT MAT & leq, 
				IN OUT MAT & vc, IN OUT INT & rhs_idx);
public:
	SIX();
	~SIX();	
	void init();
	void destroy();	
	void set_param(UINT indent, UINT max_iter = 0xFFFFFFFF);
	bool verify_empty_variable_constrain(IN MAT const& tgtf, 
										IN MAT & vc,
										IN MAT const& eq,
										IN MAT const& leq,
										IN INT rhs_idx);	
	UINT minm(OUT T & minv, 
				OUT MAT & res, 
				IN MAT const& tgtf, 
				IN MAT & vc, 
				IN MAT const& eq, 
				IN MAT const& leq,
				IN INT rhs_idx = -1); //Linear minmum solution
	UINT maxm(OUT T & maxv, 
				OUT MAT & res, 
				IN MAT const& tgtf, 
				IN MAT & vc, 
				IN MAT const& eq, 
				IN MAT const& leq,
				IN INT rhs_idx = -1); //Linear maximum solution		
	UINT calc_dual_maxm(OUT T & dual_maxv, 
						OUT MAT & dual_slack_sol,
						OUT MAT & dual_tgtf,
						OUT INT & dual_rhs_idx,
						OUT INT & dual_num_nv,
						IN MAT const& tgtf,
						IN MAT & vc, 
						IN MAT const& leq, 
						IN INT rhs_idx);
	UINT two_stage_method(IN OUT MAT & newleq, 
						IN OUT MAT & newvc, 
						IN OUT MAT & newtgtf,
						IN OUT MAT & slack_sol,
						IN OUT T & maxv,
						IN OUT SVECTOR<bool> & nvset,
						IN OUT SVECTOR<bool> & bvset,
						IN OUT SVECTOR<INT> & bv2eqmap,
						IN OUT SVECTOR<INT> & eq2bvmap,
						IN OUT INT & new_rhs_idx);	
	void calc_final_solution(OUT MAT & sol,
							OUT T & v,
							IN OUT MAT & slack_sol,
							IN INTMAT & vcmap,
							IN MAT const& orignal_tgtf,
							IN INT rhs_idx);
	void convert_eq2ineq(OUT MAT & leq, IN MAT const& eq);
	bool calc_sol(IN OUT MAT & sol, 
					IN SVECTOR<bool> const& has_val, 
					IN MAT const& eqc, 
					INT rhs_idx);
	bool is_feasible(IN MAT const& sol, 
					IN MAT & lc, 
					bool is_eqc, 
					IN MAT const& vc, 
					INT rhs_idx);		
	bool construct_basic_feasible_solution(IN OUT MAT & leq, 
										   IN OUT MAT & tgtf,
										   IN OUT MAT & vc,
										   IN OUT SVECTOR<bool> & nvset,
										   IN OUT SVECTOR<bool> & bvset, 
										   IN OUT SVECTOR<INT> & bv2eqmap,
										   IN OUT SVECTOR<INT> & eq2bvmap, 
										   IN OUT INT & rhs_idx);	
	void revise_target_func(IN OUT MAT & tgtf,
							IN MAT const& eq,
							IN MAT const& leq,							
							IN INT rhs_idx);
	CHAR const* get_status_name(UINT status) const;
	FILE * dump_open_file();
	bool dump_prt_indent(FILE * h);
	bool dump_pivoting(UINT status, UINT iter_count);
};


template <class MAT, class T>
SIX<MAT, T>::SIX()
{
	m_is_init = false;
	m_rhs_idx = -1;
	m_ppt = NULL;
	init();
}


template <class MAT, class T>
SIX<MAT, T>::~SIX()
{
  	destroy();
}


template <class MAT, class T>
void SIX<MAT, T>::init()
{
	if (m_is_init) return;
	m_indent = 0;
	m_max_iter = 0xFFFFFFFF;
	m_is_init = true;
}


template <class MAT, class T>
void SIX<MAT, T>::destroy()
{
	if (!m_is_init) return;
	m_rhs_idx = -1;
	m_is_init = false;
	if (m_ppt) {
		delete m_ppt;
	}
}


template <class MAT, class T>
void SIX<MAT, T>::set_param(UINT indent, UINT max_iter)
{
	m_indent = indent;	
	m_max_iter = max_iter;
}


//Generate new pivot-pair-tab.
template <class MAT, class T>
void SIX<MAT, T>::_new_ppt(INT var_num)
{
	IS_TRUE0(var_num > 0);
	if (m_ppt) {
		m_ppt->destroy();
		m_ppt->init(var_num);
		return;
	}
	m_ppt = new PIVOT_PAIR_TAB(var_num);
}


template <class MAT, class T>
FILE * SIX<MAT, T>::dump_open_file()
{
	FILE * h = fopen(SIX_DUMP_NAME, "a+");
	IS_TRUE(h, ("%s create failed!!!", SIX_DUMP_NAME));
	return h;
}


template <class MAT, class T>
bool SIX<MAT, T>::dump_prt_indent(FILE * h)
{
	for (UINT i = 0; i < m_indent; i++) {
		fprintf(h, "  ");
	}
	return true;
}


template <class MAT, class T>
CHAR const* SIX<MAT, T>::get_status_name(UINT status) const
{
	CHAR const* s;
	switch (status) {
	case SIX_SUCC: s = "SUCC"; break;
	case SIX_UNBOUND: s = "UNBOUND"; break;
	case SIX_NO_PRI_FEASIBLE_SOL: s = "NO Prime SOL"; break;
	case SIX_OPTIMAL_IS_INFEASIBLE: s = "Optimal SOL is infeasible"; break;
	case SIX_TIME_OUT: s = "Time Out"; break;
	default:
		IS_TRUE0(0);
	}
	return s;
}


template <class MAT, class T>
bool SIX<MAT, T>::dump_pivoting(UINT status, UINT iter_count)
{
	FILE * h = dump_open_file();
	dump_prt_indent(h);
	fprintf(h, "PIVOT status=%s,iter_cnt=%u\n",	
			get_status_name(status), iter_count);
	fclose(h);
	return true;
}


/*
Calculate solution by given variable's value, 
return true if solution is unique.

'has_val': presents which variable was depicted in 'sol'.
'eqc': equality constraint
'vc': variable constraint
*/
template <class MAT, class T>
bool SIX<MAT, T>::calc_sol(IN OUT MAT & sol, 
						IN SVECTOR<bool> const& has_val, 
						IN MAT const& eqc, 
						INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	IS_TRUE(has_val.get_last_idx() + 1 == rhs_idx && 
			sol.get_col_size() == eqc.get_col_size(), ("illegal info"));
	MAT tmpeqc = eqc;
	UINT i;

	//Calc the number of variables which has value.
	UINT hasvalnum = 0;
	for (i = 0; i < (UINT)rhs_idx; i++) {
		if (has_val.get(i)) {
			hasvalnum++;
		}
	}

	if (hasvalnum == (UINT)rhs_idx) {
		//'sol' already be solution
		return true;
	}

	//Computes the number of the variables which consist of each equations.
	//If there are more than one variable in the equation, the solution 
	//of the group equations will NOT be unique.
	SVECTOR<UINT> eq2var; //When equation was processed, record the 
						//variable index that will be solved.
	for (i = 0; i < tmpeqc.get_row_size(); i++) {
		UINT nonzerocoeff = 0;
		for (UINT j = 0; j < (UINT)rhs_idx; j++) {
			if (has_val.get(j)) {
				continue;
			}
			if (IS_INEQ(tmpeqc.reduce(i, j), 0)) {
				nonzerocoeff++;

				//Suppose each equality only have one variable require
				//to solve. And the first that is.
				eq2var.set(i, j); 
			}
		}
		if (nonzerocoeff > 1) { //magitude may be zero
			return false;
		}
	}

	//Exam OK! Compute the solution.
	for (i = 0; i < (UINT)rhs_idx; i++) {
		if (!has_val.get(i)) {
			sol.set(0, i, 0); //do some cleaning
			continue;
		}
		T val = sol.get(0, i);
		tmpeqc.mul_of_col(i, val);
	}

	//If variable has been computed, set relevant vector position to true.
	SVECTOR<bool> comped; 
	for (i = 0; i < tmpeqc.get_row_size(); i++) {
		T temval = 0, varcoeff = 0;
		INT varidx = eq2var.get(i);
		IS_TRUE(!comped.get(varidx), ("already has computed"));
		comped.set(varidx, true);
		for (UINT j = 0; j < (UINT)rhs_idx; j++) {
			if ((UINT)varidx == j) {
				varcoeff = tmpeqc.get(i, j);
				continue;
			}
			temval = temval + tmpeqc.get(i, j);
		}
		temval = tmpeqc.get(i, rhs_idx) - temval; //ax1 = C - bx2 - cx3
		varcoeff.reduce();
		if (IS_INEQ(varcoeff, 1)) {
			temval = temval/varcoeff; //x1 = (C - bx2 - cx3) / a
		}
		sol.set(0, varidx, temval);
	}
	return true;
}


/*
Calculate the value for each basic variable and find the 
minmum one to be the candidate that will be nonbasic variable.

NOTICE:
	When we are going to find the tightest constraint for 
	variable 'pivot_nv', assumed the other nonbasic variables 
	to be zero, in other word that the tightest value of 
	'pivot_nv' equals:
		const-term / pivot_nv's coeff.

	In actually, since each equality corresponding to one 
	specifical basic variable, the value of basic variable is 
	also the slack range of 'pivot_nv'.
*/
template <class MAT, class T>
INT SIX<MAT, T>::_find_pivot_bv(IN UINT pivot_nv, 
							IN OUT PV_PARM<MAT> & pp)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	MAT const& eqc = *pp.eq;
	SVECTOR<INT> const& eq2bvmap = *pp.eq2bvmap;
	SVECTOR<bool> const& nvset = *pp.nvset;
	PIVOT_PAIR_TAB & ppt = *pp.ppt;
	INT rhs_idx = pp.rhs_idx;

	IS_TRUE0(nvset.get(pivot_nv));
	T minbval;
	INT eqidx = -1;
	UINT i;
	bool all_unbound = true;
	bool first = true;
	for (i = 0; i < eqc.get_row_size(); i++) {
		T pivot_nv_val = eqc.get(i, pivot_nv);

		//Positive coeff is the one we are looking for.
		if (IS_LE(pivot_nv_val, 0)) { 
			/*
			xi is unbound!
			e.g:Given x1 is basic variable, x2 is non-basic variable, and
				the equality as:
					x1 = 28 + x2 + 0 + 0 (our form is: x1 - x2 - 0 - 0 = 28) 
				No matter how we increase the value of x2, 
				x1 is always positive, and also satified x1's 
				limits( x1 >= 0 ) at the same time. 
				We said x1 is unconstrained to x2.
				So this equality(constraint) 'i' could not limit x2.
				Similarly for situation that x2 is zero.
			*/
			continue;
		}
		
		if (ppt.is_handle(pivot_nv, eq2bvmap.get(i))) {
			//Pivoting pair had already examed.
			//Try next to avoid cycle.
			continue;
		}
		
		//Hint:nvnum + bvnum == rhs_idx
		if (!ppt.can_be_bv_cand(eq2bvmap.get(i))) {
			continue;
		}
		
		//Find candidate!
		all_unbound = false;

		T v = eqc.get(i, rhs_idx) / pivot_nv_val;
		if (first) {
			minbval = v;
			eqidx = i;
			first = false;
		} else if (minbval > v) {
			minbval = v;
			eqidx = i;
		}
	}//end for
	
	/*
	None of basic variable that could form a bound for 'pivot_nv'.
	Whereas contrasting the classical algorithm, we are going to relax
	the choice condition, that is finding a basic variable as the 
	tightest bound of 'pivot_nv', even if coeff of 'pivot_nv' is not
	positive(in our presentation).

	Well, the new policy we applied is looking for the equality with the
	minimal value of 'constant_coeff/pivot_nv's_coeff'. 
	And relative basic variable of that equality could be what we found.
	*/
	if (all_unbound) {
		eqidx = -1; //Find again! good luck!
		bool first = true;
		for (i = 0; i < eqc.get_row_size(); i++) {
			if (ppt.is_handle(pivot_nv, eq2bvmap.get(i))) {
				//Pivoting pair had already examed.
				//Try next to avoid cycle.
				continue;
			}

			//Hint:nvnum + bvnum == rhs_idx
			if (!ppt.can_be_bv_cand(eq2bvmap.get(i))) {
				continue;
			}

			T den = eqc.get(i, pivot_nv);
			if (den == 0) {
				continue;
			}

			T v = eqc.get(i, rhs_idx) / den;
			if (first) {
				minbval = v;
				eqidx = i;
				first = false;
			} else if (minbval > v) {
				minbval = v;
				eqidx = i;
			}
		}//end for	
		if (eqidx == -1) {
			//All pair of (pivot_nv_idx, bv_idx) had processed.
			//Retry another nv to aviod cycle.
			return -1;
		}
	}
	
	//We found a basic varible in condition! 
	//And it will be the tightest bound of 'pivot_nv'.
	return eq2bvmap.get(eqidx);
}


/*
Finding the feasible pivoting pair of (nv, bv).

NOTICE: 
	In order to avoid the degeneracy case, record the pivoting
	pair at a time.
*/
template <class MAT, class T>
INT SIX<MAT, T>::_find_pivot_nv_bv_pair(OUT INT & nvidx, 
									OUT INT & bvidx, 
									IN OUT PV_PARM<MAT> & pp)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	MAT const& tgtf = *pp.tgtf;
	IN OUT SVECTOR<bool> & nvset = *pp.nvset;
	IN OUT SVECTOR<bool> & bvset = *pp.bvset;						
	PIVOT_PAIR_TAB & ppt = *pp.ppt;
	INT rhs_idx = pp.rhs_idx;
	UINT i;

	//If all variables with positive coefficient are examed, 
	//try negative ones.
	bool find = false;

	//In the case of all the coeff of 'tgtf' is nonpositive, the NV with
	//negtive coeff might be recommended.
	bool try_neg_coeff_var = false;
	bool try_zero_coeff_var = false;
AGAIN:
	for (i = 0; i < (UINT)rhs_idx; i++) {
		T coeff = tgtf.get(0, i);
		if (bvset.get(i)) {
			IS_TRUE(coeff == 0, ("the coeff of 'bv' must be zero."));
			continue;
		}

		IS_TRUE0(nvset.get(i));
		
		//Selecting the nv in term of 'pair table'.
		//Here n+m == rhs_idx
		if (!ppt.can_be_nv_cand(i)) {
			/*
			Given a number of n of nonbasic variables and a 
			number of m of basic variables, there will be at 
			most (n+m-1) times to try for each nonbasic variables. 
			*/
			continue;
		}

		if (coeff > 0) {
			if (try_neg_coeff_var) {
				continue;
			}
			/*
			Calculate the value for each basic value and find the 
			minmum one as pivoting basic candidate.
			*/
			bvidx = _find_pivot_bv(i, pp);
			if (bvidx == -1) {
				/*
				Can not find any basic variable to swap out.
				Try another non-basic	variable.
				start_idx_of_v = pivot_nv_idx + 1;
				*/
				continue;
			}
			nvidx = i;
			goto FIN;
		} else if (coeff == 0) {
			if (!try_zero_coeff_var) {
				continue;
			}
			bvidx = _find_pivot_bv(i, pp);
			if (bvidx == -1) {
				continue;
			}
			nvidx = i;
			goto FIN;
		} else { //Tring variable with negative coeff.
			if (!try_neg_coeff_var) {
				continue;
			}
			bvidx = _find_pivot_bv(i, pp);
			if (bvidx == -1) {
				continue;
			}
			nvidx = i;
			goto FIN;
		}
	}//end for
	
	if (!find) {
		if (!try_zero_coeff_var) {
			try_zero_coeff_var = true;
			goto AGAIN;
#ifdef ALLOW_RELAX_CURRENT_SOL
		} else if (!try_neg_coeff_var) {
			/*
			All of the variables with positive coefficient were tried.
			Attempt the negative one to next. But this will cause the
			relaxation the optimal solution!			
			Or the scenario should be solved during initialize-simplex.
			I wonder if it is a good idea.
			*/
			try_neg_coeff_var = true;
			goto AGAIN;
#endif
		} else {
			return false;
		}
	}
	IS_TRUE(0, ("arrive in here?"));
FIN:
	return true;
}



/*
Return true if 'sol' is feasible, otherwise return false.

'sol': solution to exam.
'lc': linear constraint.
'is_eqc': True indicate 'lc' is equality constraint, 
	and False indicate lc is inequality constraint.	
'vc': variable constraint
*/
template <class MAT, class T>
bool SIX<MAT, T>::is_feasible(IN MAT const& sol, 
							IN MAT & lc, 
							bool is_eqc,
							IN MAT const& vc, 
							INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	IS_TRUE(sol.get_col_size() == lc.get_col_size() && 
			lc.get_col_size() == vc.get_col_size(), 
			("illegal info"));
	UINT i;

	//Check variable constraint
	for (i = 0; i < (UINT)rhs_idx; i++) {
		if (vc.get(i, i) * sol.get(0, i) > vc.get(i, rhs_idx)) {
			return false;
		}
	}
	
	//Check equality constraint
	for (i = 0; i < lc.get_row_size(); i++) {
		T sum = T(0);
		for (UINT j = 0; j < (UINT)rhs_idx; j++) {
			sum = sum + lc.get(i, j) * sol.get(0, j);
		}
		if (is_eqc) {
			sum.reduce();
			if (IS_INEQ(sum, lc.reduce(i, rhs_idx))) {
				return false;
			}
		} else {
			if (sum > lc.get(i, rhs_idx)) {
				return false;
			}
		}
	}
	return true;
}


/*
Construct a basic feasible solution.

Return true if we have found the slack form of a feasible solution,
otherwise there is no feasible solution of original problem.

NOTICE:
	Sometimes we could not find out the basic feasible solution.
	Therefore we attempt to reconstruct original problem L into a 
	auxiliary one L'. The solution of L' is equivalent to L.
	In this funtion, we introduced a assistant variable x0, that 
	constraint is x0��0, and the target function is L'={x0|max(-x0)}. 
	And if L' has a maximum feasible solution: max(-x0)=0, x0=0,
	the slack form exists a feasible solution and L also has a solution.	
*/
template <class MAT, class T>
bool SIX<MAT, T>::construct_basic_feasible_solution(IN OUT MAT & leq, 
											IN OUT MAT & tgtf,
											IN OUT MAT & vc,
											IN OUT SVECTOR<bool> & nvset, 
											IN OUT SVECTOR<bool> & bvset, 
											IN OUT SVECTOR<INT> & bv2eqmap,
											IN OUT SVECTOR<INT> & eq2bvmap,
											IN OUT INT & rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	IS_TRUE(rhs_idx > 0, ("illegal info"));

	/*
	Construct auxiliary linear programming L' by introducing one
	nonnegative variable 'xa', and the target function was transformed as:
		max(x) = ax1 + bx2 + ... + zxn + (-xa), where xa >= 0,
		x1,x2,...,xn,xa >= 0
	*/
	MAT origtgtf = tgtf;
	INT orig_rhs_idx = rhs_idx;

	//Append columns for auxiliary variable 'xa'
	leq.insert_col_before(rhs_idx);
	leq.set_col(rhs_idx, -1);
	tgtf.insert_col_before(rhs_idx);
	tgtf.zero();
	tgtf.set_col(rhs_idx, -1);
	vc.insert_col_before(rhs_idx);
	vc.grow_row(1);
	vc.set(rhs_idx, rhs_idx, -1);
	vc.set(rhs_idx, rhs_idx + 1, 0);

	UINT xa = rhs_idx; //variable index of 'xa'	
	rhs_idx++;	
	UINT bv_start_idx = rhs_idx;

	//Slack transformation of L'.
	_slack(tgtf, leq, vc, rhs_idx);
	
	/*
	Initializing basic-variable set and nonbasic-variable set.
	Establishing the mapping between variables and relevant equalities.
	*/
	UINT i,j;
	for (i = 0; i < bv_start_idx; i++) {
		bv2eqmap.set(i, INVALID_EQNUM); 
		nvset.set(i, true); //Initializing nonbasic variable set.
	}
	j = 0; //Index that of mapping of bv and eq starting at 0.
	for (i = bv_start_idx; i < (UINT)rhs_idx; i++, j++) {
		bvset.set(i, true);
		nvset.set(i, false);
		eq2bvmap.set(j, i); 
		bv2eqmap.set(i, j); 
	}

	/*
	Find the minimum constant term, and select the basic-variable 
	to be the swap-out one.
	*/
	UINT pivot_bv_idx;
	T minb;
	for (i = 0; i < leq.get_row_size(); i++) {
		if (i == 0) {
			minb = leq.get(i, rhs_idx);
			pivot_bv_idx = eq2bvmap.get(i);
		} else if (minb > leq.get(i, rhs_idx)) {
			minb = leq.get(i, rhs_idx);
			pivot_bv_idx = eq2bvmap.get(i);
		}
	}
	
	PV_PARM<MAT> pp(&leq, &tgtf, &nvset, &bvset, 
					&bv2eqmap, &eq2bvmap, NULL, rhs_idx);
	_pivot(xa, pivot_bv_idx, pp);
	
	MAT sol;
	T maxv;
	if (SIX_SUCC != _solve_slack_form(tgtf, leq, vc, maxv, sol,
							nvset, bvset, bv2eqmap, eq2bvmap, rhs_idx)) {
		return false;	
	}
	
	/*
	Maximum solution of L' must be zero, or else L has no basic 
	feasible solution.
	*/
	maxv.reduce();
	if (IS_INEQ(maxv, 0)) {
		return false;
	}
	
	if (bvset.get(xa)) {
		/*
		CASE: 'xa' might be basic variable! 
		In the sake of that each BV is relevant to an unique equation, 
		perform pivoting to pivot xa to a nv.
		*/
		INT cand_nv;
		INT eqnum = bv2eqmap.get(xa);
		IS_TRUE(eqnum >= 0, ("index must nonnegative"));
		for (cand_nv = 0; cand_nv <= nvset.get_last_idx(); cand_nv++) {
			if (nvset.get(cand_nv) && 
				IS_INEQ(leq.reduce(eqnum, cand_nv), 0)) {
				break;
			}
		}
		IS_TRUE(cand_nv <= nvset.get_last_idx(), 
				("No candidate nv can be used in pivoting"));
		_pivot(cand_nv, xa, pp);
		IS_TRUE0(!bvset.get(xa));
	}

	//Updating original target function by substituting xa with 0.
	UINT inccols = tgtf.get_col_size() - origtgtf.get_col_size();
	tgtf = origtgtf;
	tgtf.insert_cols_before(orig_rhs_idx, inccols);
	for (i = 0; i < (UINT)rhs_idx; i++) {
		if (IS_INEQ(tgtf.reduce(0, i), 0) && bvset.get(i)) {
			MAT exp;
			INT ii = bv2eqmap.get(i);
			leq.inner_row(exp, bv2eqmap.get(i), bv2eqmap.get(i));
			tgtf.substit(exp, i, false, rhs_idx);
		}
	}

	//Remove xa relevant information.
	tgtf.del_col(xa);
	leq.del_col(xa);
	vc.del_col(xa);
	vc.del_row(xa);

	//Update the bvset, nvset, and correspondence between bv and eqnum.
	j = 0;
	SVECTOR<bool> tnvset, tbvset; //local use.
	SVECTOR<INT> tbv2eqmap;
	for (i = 0; i <= (UINT)nvset.get_last_idx(); i++) {
		if (i != xa) {
			tnvset.set(j, nvset.get(i));
			tbvset.set(j, bvset.get(i));
			tbv2eqmap.set(j, bv2eqmap.get(i));
			j++;
		}
	}
	nvset.copy(tnvset);
	bvset.copy(tbvset);
	bv2eqmap.copy(tbv2eqmap);

	//Update the correspondence between eqnum and bv.
	for (i = 0; i <= (UINT)eq2bvmap.get_last_idx(); i++) {
		UINT bv = eq2bvmap.get(i);
		if (bv > xa) {
			eq2bvmap.set(i, bv - 1);
		}
	}

	//Update column of constant term.
	rhs_idx--;
	return true;
}


/*
Solve slack-form that given by 'eqc'.
Return the maximal value and solution, otherwise return unbound.

'tgtf': target function
'eqc': slack form
'vc': variable constraint
'maxv': maximum solution

NOTICE:
	The slack form can be illustrated as:
		10x + 11y + 60z �� min
		-0.1x + (-1.2y) + (-3.3z) + s1 = -50
		-73x + (-96y) + (-253z) + s2 = -3200
		-9.6x + (-7y) + (-19z) + s3 = -1000
	and variable constraint:
		x,y,z,s1,s2,s3 >= 0
*/
template <class MAT, class T>
UINT SIX<MAT, T>::_solve_slack_form(IN OUT MAT & tgtf, 
							IN OUT MAT & eqc,
							IN OUT MAT & vc, 
							OUT T & maxv,
							OUT MAT & sol,
							IN OUT SVECTOR<bool> & nvset, 
							IN OUT SVECTOR<bool> & bvset,
							IN OUT SVECTOR<INT> & bv2eqmap, 
							IN OUT SVECTOR<INT> & eq2bvmap, 
							IN INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	_new_ppt(rhs_idx);

	/*
	The initial-solution of target function is always be zero, 
	we get the solution by setting each of non-basic variables to be zero.
	e.g: max(x) = ax1 + bx2, 
		Set x1, x2 to be zero, then the max(x) is a*0 + b*0 = 0.
	*/
	maxv = 0;
	sol.reinit(1, tgtf.get_col_size());	

	UINT cnt = 0; //The number of iterations
	PV_PARM<MAT> pp(&eqc, 
					&tgtf, 
					&nvset, 
					&bvset, 
					&bv2eqmap, 
					&eq2bvmap, 
					m_ppt,
					rhs_idx);
	while (cnt < m_max_iter) {
		{
			//m_ppt->m_pair.dumpf();
			//tgtf.dumpf();
			//eqc.dumpf();
			//Dumpf_Svec((void*)&nvset, D_BOOL);
			//Dumpf_Svec((void*)&bvset, D_BOOL);
			//Dumpf_Svec((void*)&bv2eqmap, D_INT);
			//Dumpf_Svec((void*)&eq2bvmap, D_INT);.dumpf();
		}
		/*
		Choose the nonbasic variable that coefficient is 
		positive to be the pivoting candidate.
		*/
		INT pivot_nv_idx = -1;
		bool all_coeffs_nonpositive = true;
		for (UINT i = 0; i < (UINT)rhs_idx; i++) {
			if (!nvset.get(i)) {
				//May be the verify is unnecessary.
				IS_TRUE(IS_EQ(tgtf.reduce(0, i), 0), 
						("bv must be zero in target function"));
				tgtf.set(0, i, 0);
				continue;
			}
			if (tgtf.get(0, i) > 0) {
				all_coeffs_nonpositive = false;
				if (m_ppt->can_be_nv_cand(i)) {
					pivot_nv_idx = i;
					break;
				}
			} //end if
		} //end for	each NV

		INT pivot_bv_idx = -1;
		if (pivot_nv_idx == -1) {
			/*
			Each coeffs of target function either be zero or negaitive!
			It looks like that we could not gain more large value of target
			function by increase value of one of its variables. 
			So it has to perform feasibility verification.

			Now, we attempt to calculate the optimial solution, 
			and target function was represented by Non-basic variable all
			through. 
			So one simply method to get value of basic variable is 
			that setting Non-basic variable all be zero.
			And the basic variables could consequently be worked out.
			The following code would do that operation accroding 
			to information in 'bvset'.

			There are two possibilities,  one is that the optimal solution
			is avaiable, and the else is that the solution is infeasible.
			*/
			if (all_coeffs_nonpositive) {
				sol.zero(); //Set all nonbasic variables to zero.

				/*
				Each basic variables BVi is equal to corresponding Ci.
				e.g: Given slack form is:
					BVi = Ci + f1(NVi)
					BVj = Cj + f2(NVi)
				then BVi, BVj are equal to Ci, Cj respectively.	
				*/
			#ifdef _DEBUG_
				//Also do some verifying work but is slow 
				//and need more calculation.
				if (!calc_sol(sol, nvset, eqc, rhs_idx)) {
					IS_TRUE(0, ("exception!!"));
				}
			#else
				for (INT i = 0; i <= bvset.get_last_idx(); i++) {
					if (bvset.get(i)) {
						INT eqnum = bv2eqmap.get(i);
						IS_TRUE0(eqnum >= 0);
						sol.set(0, i, eqc.get(eqnum, rhs_idx));
					}
				}
			#endif
				if (is_feasible(sol, eqc, true, vc, rhs_idx)) {
					/*
					Calculating maximum value.
					It should be noted that SIX only support linear 
					expression with single column constant term so far.
					e.g: maxv = ax1 + bx2 + Const, 
					The followed form should be considered,
						maxv = ax1 + bx2 + Const + ConstSym(x).
					*/
					maxv = tgtf.get(0, rhs_idx);
					IS_TRUE0(dump_pivoting(SIX_SUCC, cnt));
					return SIX_SUCC;
				} else {
					//IS_TRUE(0, 
					//	("solution is either feasibly optimal or unbound!"));
					IS_TRUE0(dump_pivoting(SIX_OPTIMAL_IS_INFEASIBLE, cnt));
					return SIX_OPTIMAL_IS_INFEASIBLE;
				}
			}

			/*
			Select a bv in terms of the giving nv.
			Note that the candidate 'pivot_bv_idx' is 
			relevant with the tightest constraint of 
			'pivot_nv_idx', and intrducing the tightest
			constraint into target function to get arrive 
			to the extremal point(max|min).
			If we could not find a constraint so as to 
			reduce the limitation of nv, the slack form is unbound.
			*/
			if (!_find_pivot_nv_bv_pair(pivot_nv_idx, pivot_bv_idx, pp)) {
				IS_TRUE0(dump_pivoting(SIX_UNBOUND, cnt));
				return SIX_UNBOUND;
			}
		} else {
			//Calculate the value for each basic value and find the 
			//minmum one as pivoting basic candidate.
			pivot_bv_idx = _find_pivot_bv(pivot_nv_idx, pp);
			if (pivot_bv_idx == -1) {
				//Can not find any basic variable to swap out.
				//Try another non-basic variable.
				m_ppt->disable_nv(pivot_nv_idx);
				continue;
			}
		}

		IS_TRUE0(pivot_nv_idx != -1 && pivot_bv_idx != -1 && 
				!m_ppt->is_handle(pivot_nv_idx, pivot_bv_idx));
		m_ppt->gen_pair(pivot_nv_idx, pivot_bv_idx);

		/*
		'pivot_nv_idx' refers to 'swap-in' variable, as well as the variables 
		inferred by column index.
		'pivot_bv_idx' refers to 'swap-out' variable, as well as the variables
		inferred by row index.
		Since our algorithme is definitely, there may be exist a cyclic when 
		pivoting a pair of 'swap-in', 'swap-out' variables.
			e.g: Assuming that at time t1, the swap-in is xi, swap-out is xj, 
			and at time t2, the swap-in may become xj, swap-out  xi.
				This is one cycle!
		So the important work is to find mutually exclusive pivoting variables
		pairs.
		*/
		
		_pivot(pivot_nv_idx, pivot_bv_idx, pp);
		#ifdef _DEBUG_		
		{
			UINT count = 0;
			for (UINT i = 0; i <= (UINT)bvset.get_last_idx();i++) {
				IS_TRUE(bvset.get(i) != nvset.get(i), ("illegal pivoting"));
				if (bv2eqmap.get(i) != INVALID_EQNUM) {
					count++;
					IS_TRUE((UINT)eq2bvmap.get(bv2eqmap.get(i)) == i, 
							("unmatch!")); 
				}
			}
			IS_TRUE((UINT)eq2bvmap.get_last_idx() + 1 == 
					eqc.get_row_size(), ("illegal pivot"));
			IS_TRUE(count == eqc.get_row_size(), ("illegal pivot"));
		}
		#endif
		cnt++;
	} //end while(status)	 
	IS_TRUE0(dump_pivoting(SIX_TIME_OUT, cnt));
	return SIX_TIME_OUT;
}


/*
Convert equations to equivalent inequalities, and remove
equations by algebraic substitution.
*/
template <class MAT, class T>
void SIX<MAT, T>::convert_eq2ineq(OUT MAT & leq, IN MAT const& eq)
{
#if 1
	/*
	Substitute equalitions into inequality system.
	*/
	if (eq.size() == 0) { return; }
	SVECTOR<bool> eq_removed;
	UINT eq_count = eq.get_row_size();
	if (leq.size() > 0) {
		for (INT j = 0; j < m_rhs_idx; j++) {
			UINT num_nonzero = 0;
			UINT pos_nonzero = 0;
			for (UINT i = 0; i < eq.get_row_size(); i++) {
				if (eq_removed.get(i)) {
					continue;
				}
				if (eq.get(i, j) != 0) {
					num_nonzero++;
					pos_nonzero = i;
				}
			}
			if (num_nonzero == 1) {
				eq_removed.set(pos_nonzero, true);
				eq_count--;

				/*
				Substitute variable with linear polynomials.
				e.g: Given -2i+N<=0, 0=-i+4j-3N-1
				substitute i with 4j-3N-1, one will get:
					-2(4j-3N-1)+N<=0, then simplied to -8j+7N+2<=0.		
				*/
				MAT tp;
				for (UINT m = 0; m < leq.get_row_size(); m++) {
					T v = leq.get(m, j);
					if (v == T(0)) {
						continue;
					}
					eq.inner_row(tp, pos_nonzero, pos_nonzero);
					T tpv = tp.get(0, m);
					if (tpv != T(1)) {
						tp.mul_of_row(0, T(1)/tpv);
					}
					tp.mul_of_row(0, v);
					leq.set(m, j, T(0));
					/*	
					Convert the sign of element by doing negtive 
					operation from column 'rhs_idx' to last one, 
					since these elements will be added to RHS of 
					inequalities.
					e.g: Given i=2j-1+N, convert -1+N to 1-N.
					*/
					for (UINT k = m_rhs_idx; k < tp.get_col_size(); k++) {
						tp.set(0, k, -tp.get(0, k));
					}
					leq.add_row_to_row(tp, 0, m);			
				}
			} //end if
		} //end for
	} //end if

	if (eq_count > 0) {
		UINT c = leq.get_row_size();
		if (leq.size() == 0) { 
			leq.reinit(eq_count * 2, eq.get_col_size()); 
		}
		else { leq.grow_row(eq_count*2); }
		for (UINT i = 0; i < eq.get_row_size(); i++) {
			if (eq_removed.get(i)) {
				continue;
			}
			leq.set_rows(c, c, eq, i);
			leq.mul_of_row(c, -1);
			leq.set_rows(c+1, c+1, eq, i);
			c+=2;
		}
	}
#else
	if (eq.size() != 0) {
		leq.grow_row(eq, 0, eq.get_row_size() - 1);
		UINT rows = leq.get_row_size();
		leq.grow_row(eq, 0, eq.get_row_size() - 1);
		for (UINT j = rows; j < leq.get_row_size(); j++) {
			leq.mul_of_row(j, -1);	
		}
	}
#endif
}


/*
Normalize linear programming.
	1. Converting equality constraint into inequality constraint.
	2. To ensure variables have positive bound.	
Return the starting column index of constant term. 

NOTICE:
	Some new auxillary/dummy variables may be generated.
	Caller must ensure that 'leq' does not contain constraints of '>='.
*/
template <class MAT, class T>
INT SIX<MAT, T>::_normalize(OUT MAT & newleq, 
				   OUT MAT & newvc, 
				   OUT INTMAT & vcmap,
				   OUT MAT & newtgtf,
				   IN MAT & vc, 
				   IN MAT const& eq,
				   IN MAT const& leq, 
				   IN MAT const& tgtf)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	UINT col_last_var = m_rhs_idx - 1;
	UINT vars = col_last_var + 1;	
	MAT tmpleq(leq);
	convert_eq2ineq(tmpleq, eq);	
	
	/*
	Ensuring variable has nonnegative constraint.
	e.g: Given 'vc' as follows:
		v0  v1  v2  C
	   -1   0   0   0
	    0   0   0   0
		0   0  -1   0
	it means that given constraints are -v0 <= 0 , -v2 <= 0, 
	but there is no knownledge about v1�� thus v1 might be 
	negative! 
	Thus here introduce new auxillary/dummy variables v1' and v1'',
	which satified a new equation: v1 = v1' - v1'', where v1' >= 0, 
	v1'' >= 0.
	*/
	UINT leqgrowcols = 0, tgtfgrowcols = 0;
	UINT vcgrowrows = 0, vcgrowcols = 0;
	UINT i;
	for (i = 0; i <= col_last_var; i++) {
		if (vc.is_colequ(i, 0)) { //No constraint for variable 'i'
			vcgrowrows++;
			vcgrowcols++;
			leqgrowcols++;
			tgtfgrowcols++;
		} else {
			/*
			Variable should only have one nonnegative constraint.
			e.g: xi <= N, so it was necessary to generate leq 
			constraints.
			*/
			for (UINT j = 0; j < vc.get_row_size(); j++) {
				if (vc.get(j, i) > 0) {
					IS_TRUE(0, ("constraint for No.%d variable is negative."
						" Set the variable unbound and insert v<=0 "
						"into inequalities please.", i));
				}
			}
		}
	}

	//Extract the variable-constraints which coefficient is not zero.
	vc.inner(newvc, 0, 0, vc.get_row_size() - 1, col_last_var);
	if (vcgrowcols > 0) {
		newvc.grow_col(vcgrowcols); //for new dummy variables.
	}

	//So we require the number of 'vcgrowrows' auxillary/dummy variables.
	newvc.grow_col(vc, col_last_var + 1, vc.get_col_size() - 1); 
	if (vcgrowrows > 0) {
		newvc.grow_row(vcgrowrows); //for new auxillary variables
	}
	
	//Inserting auxillary/dummy variables.
	newleq = tmpleq;
	newleq.insert_cols_before(m_rhs_idx, leqgrowcols);

	//Growing target function for substituting an original unbounded
	//variable with new auxillary/dummy variables.
	newtgtf = tgtf;
	newtgtf.insert_cols_before(m_rhs_idx, tgtfgrowcols);
	
	//Constructing an injective mapping between original variables and
	//auxillary/dummy variables.
	vcmap.reinit(0,0);
	for (i = 0; i < vars; i++) {
		if (vc.is_colequ(i, 0)) { //No constraint of variable 'i'
			/*
			Aforementioned formual wrote: v = v' - v'', 'newvc'
			replaced v with v' and inserted a new column indicating the v'',
			therefore the colum 'i' and 'col_last_var + 1' portray the 
			variable v' and v'', and that the coefficient are both -1.
			constraint of v' is: -v' <= 0, of v'' is -v'' <= 0
			*/
			newvc.set(i, i, -1);
			newvc.set(col_last_var + 1, col_last_var + 1, -1);

			//Record an injective mapping.
			INTMAT m(1,3);
			m.sete(3, i, i, col_last_var + 1);
			vcmap.grow_row(m);
			
			//Inserting new dummy variables in inequality matrix.
			MAT n;
			tmpleq.inner_col(n, i, i);
			newleq.set_col(col_last_var + 1, n);
			newleq.mul_of_col(col_last_var + 1, -1);
			
			//Inserting new dummy variables in target-function.
			tgtf.inner_col(n, i, i);
			newtgtf.set_col(col_last_var + 1, n);
			newtgtf.mul_of_col(col_last_var + 1, -1);
			col_last_var++;
		} //end if no contrain of i
	} //end for each vars
	return col_last_var + 1;
}


/*
Construct slack-form by inserting slack variable for each of inequlities.

'tgtf': target function
'leq': slack form
'vc': variable constraints

NOTICE:
	For any constraint: f <= c, we can represent it with
	the else two constraints as: f + s = c, s >= 0, where s is 
	slack variable.
*/
template <class MAT, class T>
void SIX<MAT, T>::_slack(IN OUT MAT & tgtf,
				IN OUT MAT & leq,
				IN OUT MAT & vc,
				IN OUT INT & rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));	
	UINT orig_leqs = leq.get_row_size(); 
	leq.insert_cols_before(rhs_idx, orig_leqs);
	vc.insert_cols_before(rhs_idx, orig_leqs);
	vc.grow_row(orig_leqs);
	
	//coefficient of slack variables in 'tgtf' are 0 when initialized.
	tgtf.insert_cols_before(rhs_idx, orig_leqs);

	/*
	Set diagonal elements to -1.
	The each rows of 'vc' portrays one of the variables constraint.
	e.g:  -x1  0     0   <= 0
	      0    -x2   0   <= 0
	      0    0     -x3 <= 0
	*/
	for (UINT i = 0; i < orig_leqs; i++) {
		leq.set(i, rhs_idx + i, 1);
		vc.set(rhs_idx + i, rhs_idx + i, -1);
	}

	//Amendenting the starting column index of constant part 
	rhs_idx += orig_leqs; 
}


/*
Pivot row-tableaux of normalized-form of basis-variable.

'eq': equalities in normalized form which contained slack variables.
'nv': index of non-basis variable that swap-in.
'bv': index of basis variable that swap-out.
'tgtf': target function.
'nvset': a set of non-basic variables.
'bvset': a set of basic variables.
'bv2eqmap': mapping from index of basic-variable to the
	relevant equation.
'nv2eqmap': mapping from index of nonbasic-variable to the
	relevant equation.
'rhs_idx': index of starting column of constant part.

NOTICE:
	The system of equations must be represented in normalized form.
	A pivoting of a m*n tableaux requires one variable swapping, 
	one division, m*n-1 multiplication, m*n-m-n+1 addition and n-1 
	sign change.
*/
template <class MAT, class T>
void SIX<MAT, T>::_pivot(IN UINT nv, 
						IN UINT bv,
						IN OUT PV_PARM<MAT> & pp)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	MAT & eq = *pp.eq;
	MAT & tgtf = *pp.tgtf;
	SVECTOR<bool> & nvset = *pp.nvset;
	SVECTOR<bool> & bvset = *pp.bvset;
	SVECTOR<INT> & bv2eqmap = *pp.bv2eqmap;
	SVECTOR<INT> & eq2bvmap = *pp.eq2bvmap;
	INT rhs_idx = pp.rhs_idx;

	//The index of equations starting at 0.
	INT eqnum = bv2eqmap.get(bv); 
	IS_TRUE(eqnum >= 0, ("index must nonnegative"));
	IS_TRUE0(IS_INEQ(eq.reduce(eqnum, nv), 0));
	eq.mul_of_row(eqnum, 1/(eq.get(eqnum, nv)));

	MAT nvexp; 
	eq.inner_row(nvexp, eqnum, eqnum);

	//Coeff of 'nv' of 'eq' is '1'.
	//Substitute each nv for all equations with 
	//a expression (ax1 + bx2 + cx3) such as:
	//  nv = ax1 + bx2 +cx3
	UINT i;
	for (i = 0; i < eq.get_row_size(); i++) {
		if (i == (UINT)eqnum) {
			continue;
		}
		T coeff_of_nv = -eq.get(i, nv);
		for (UINT j = 0; j < eq.get_col_size(); j++) {
			T v = coeff_of_nv * nvexp.get(0, j);
			eq.set(i, j, eq.get(i, j) + v);
		}
	}

	/*
	Substitute variables 'nv' in target function with its expression 
	such as:
	f(x) = w*nv + u*x4 =>
	f(x) = w*(a*x1 + b*x2 + c*x3) + u*x4
	*/
	nvexp.mul(-1);
	for (i = rhs_idx; i < nvexp.get_col_size(); i++) {
		nvexp.set(0, i, -nvexp.get(0, i));
	}
	nvexp.mul(tgtf.get(0, nv));
	tgtf.add_row_to_row(nvexp, 0, 0);
	
	//Switch the role of 'nv' and 'bv', and update relevant information.
	nvset.set(nv, false);
	nvset.set(bv, true);
	bvset.set(nv, true);
	bvset.set(bv, false);
	eq2bvmap.set(bv2eqmap.get(bv), nv);
	bv2eqmap.set(nv, bv2eqmap.get(bv));
	bv2eqmap.set(bv, INVALID_EQNUM);
}


/*
Verify the legality of input data structures and 
initialize the constant-term column.
*/
template <class MAT, class T>
void SIX<MAT, T>::_verify(IN MAT const& leq, 
						IN MAT const& eq, 
						IN MAT const& tgtf, 
						IN MAT const& vc,
						IN INT rhs_idx)
{
	IS_TRUE(rhs_idx == -1 || 
			rhs_idx == (INT)leq.get_col_size() -1, ("unsupport"));
	INT max_cols = -1;
	if (eq.size() != 0 && leq.size() != 0) {
		IS_TRUE(eq.get_col_size() == leq.get_col_size(), 
				("unmatch variables"));
		max_cols = eq.get_col_size();
	} else if (eq.size() != 0) {
		max_cols = eq.get_col_size();
	} else if (leq.size() != 0) {
		max_cols = leq.get_col_size();
	} else {
		IS_TRUE(0, ("no constraints"));
	}
	IS_TRUE(rhs_idx == -1 || rhs_idx == (INT)leq.get_col_size() -1, 
			("unsupport"));

	if (m_rhs_idx == -1) {
		m_rhs_idx = max_cols - 1; //Only one const-term.
	} else {
		IS_TRUE(m_rhs_idx < (INT)max_cols && m_rhs_idx >= 1, 
				("out of boundary"));
	}

	UINT num_cols_of_const_term = max_cols - m_rhs_idx; 
	IS_TRUE(num_cols_of_const_term == 1, 
			("No yet support const term with multi-columns."));
	IS_TRUE(tgtf.is_vec() && tgtf.get_col_size() == (UINT)max_cols,
			("multi target functions"));
	IS_TRUE(vc.get_row_size() == (UINT)m_rhs_idx && 
			vc.get_col_size() == (UINT)max_cols, 
			("unmatch variables constraints"));
}


/*
Calculate the dual maximization solution to solve original minimum problem.
Assuming the followed set in order to clarify the method,
	Ns is the set of NonBasic Var with n elements,	
	Bs is the set of Basic Var with m elements, 
	X1 is the set of real var with n elements,
	X2 is the set of auxillary var with m elements.	
Also assuming original target function is of the form:
		min() = ��Gi*Yi, i = (0,1,...,m),
note that each auxillary var of maxm-problem is corresponding to 
the real var of original minm-problem.

The derivation of the original solution conforms to the formula that if 
the final form of the dual target function was shown as:
		max() = maxv + ��Cj*Xj, j��Ns, Cj��0
then the each elements of original solution is corresponding to 
the relevant coefficient of element which is of X2, namely, 
		Yi = -(C'i), where C'i��X2, i = (0,1,...,m).

'leq': must be formed of : A��y��c��
'tgtf': original target function, b��y
'vc': variable constraint of y.
'rhs_idx': the starting column of const-term of 'leq'.

NOTICE: 
	The duality theory: Ax��b, max(cx) is the dual of A��y��c��, min(b��y)
*/
template <class MAT, class T>
UINT SIX<MAT, T>::calc_dual_maxm(OUT T & dual_maxv, 
								OUT MAT & dual_slack_sol,
								OUT MAT & dual_tgtf,
								OUT INT & dual_rhs_idx,
								OUT INT & dual_num_nv,
								IN MAT const& tgtf,
								IN OUT MAT & vc, 
								IN MAT const& leq, 
								IN INT rhs_idx)
{
	INT num_of_const_col = leq.get_col_size() - rhs_idx;
	IS_TRUE0(num_of_const_col >= 1);
	
	//The constraint of minimum problem is -A��y��-c��, y��0.
	//Generate the constraint of corresponding max-problem.	
	MAT A, b, c;
	leq.inner_col(A, 0, rhs_idx - 1); 
	A.trans();
	MAT dual_leq(A);
	dual_leq.grow_col(num_of_const_col);
	dual_leq.mul(-1); //gen [A, 0]

	//gen [A, b]
	for (INT i = 0; i < rhs_idx; i++) {
		dual_leq.set(i, dual_leq.get_col_size() - num_of_const_col, 
					tgtf.get(0, i));
	}		

	//Generate [c], the coeff of target function of max-problem.
	leq.inner_col(dual_tgtf, rhs_idx, leq.get_col_size() - num_of_const_col);
	dual_tgtf.trans();
	dual_tgtf.grow_col(num_of_const_col);
	dual_tgtf.mul(-1);

	//Column of const-term
	dual_rhs_idx = dual_tgtf.get_col_size() - num_of_const_col;
	dual_num_nv = dual_rhs_idx;

	//constraints of dual variable 
	MAT dual_vc; 
	dual_vc.grow_all(dual_rhs_idx , dual_rhs_idx );
	dual_vc.eye(-1); //dual variable must be nonnegative one.
	dual_vc.grow_col(num_of_const_col);

	//*********************
	//Starting solve the dual problem
	//*********************
	//dual bv2eqmap, eq2bvmap
	SVECTOR<INT> dual_bv2eqmap, dual_eq2bvmap; 
	SVECTOR<bool> dual_nvset; //Nonbasis variable set
	SVECTOR<bool> dual_bvset; //Basis variable set
	UINT status = two_stage_method(dual_leq, 
									dual_vc, 
									dual_tgtf,
									dual_slack_sol,
									dual_maxv,
									dual_nvset,
									dual_bvset,
									dual_bv2eqmap,
									dual_eq2bvmap,
									dual_rhs_idx);
	if (status == SIX_SUCC) {
		//Do not forget the const-term.
		for (UINT i = rhs_idx; i < tgtf.get_col_size(); i++) {
			dual_maxv = dual_maxv + tgtf.get(0, i);
		}
	}
	return status;
}


#define SOLVE_DUAL_PROBLEM
#ifdef SOLVE_DUAL_PROBLEM
/*
Solve the minmum problem of linear prgramming.
*/
template <class MAT, class T>
UINT SIX<MAT, T>::minm(OUT T & minv, 
					OUT MAT & sol, 
					IN MAT const& tgtf, 
					IN OUT MAT & vc, 
					IN MAT const& eq, 
					MAT const& leq, 
					IN INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	m_rhs_idx = rhs_idx;
	_verify(leq, eq, tgtf, vc, rhs_idx);
	IS_TRUE0(m_rhs_idx > 0);

	MAT nd_leq, nd_vc, nd_tgtf; //normalized 'leq', 'vc', 'tgtf'.	
	//record variable's mapping if that exist unconstrained variable.
	INTMAT orig_vcmap;
	
	//First, normalize the minimum problem,
	//and record the column of const-term after normalizing.
	INT nd_rhs_idx = _normalize(nd_leq, nd_vc, orig_vcmap, 
								nd_tgtf, vc, eq, leq, tgtf);
	IS_TRUE(nd_rhs_idx > 0, ("at least one variable"));

	//Calclate dual problem solution.
	MAT dual_slack_sol, dual_tgtf;
	INT dual_rhs_idx = -1;
	INT dual_num_nv = -1; //record the number of dual-nv
	T dual_maxv = 0;
	minv = 0;
	UINT status = calc_dual_maxm(dual_maxv,
								dual_slack_sol,
								dual_tgtf,
								dual_rhs_idx,
								dual_num_nv,
								nd_tgtf,
								nd_vc,
								nd_leq, 
								nd_rhs_idx);
	if (status == SIX_SUCC) {
		INT num_of_const_term = dual_tgtf.get_col_size() - dual_rhs_idx;		
		//yi = -Coeff(n+i), i��Bs
		UINT num_of_orig_var = //also be the number of dual-bv.
				(dual_tgtf.get_col_size() - num_of_const_term) - dual_num_nv;

		/*
		Basic variables of dual solution represent 
		the nonbasic variables of original solution.
		e.g:If the dual-slack-solution is:
			{x0, x1, x2, ... x(n), y0, y1, ... ym, C}
		the original-solution should be the form that: 
			{y0, y1, ... ym, C}
		*/
		MAT orig_tmp_slack_sol(1, num_of_orig_var + num_of_const_term); 
		for (UINT k = 0; k < num_of_orig_var; k++) {
			orig_tmp_slack_sol.set(0, k, -dual_tgtf.get(0, dual_num_nv + k));
		}

		//minimum value
		T v;
		calc_final_solution(sol, v, 
							orig_tmp_slack_sol, 
							orig_vcmap, 
							tgtf, 
							nd_rhs_idx);
		v.reduce();
		dual_maxv.reduce();
		sol.reduce();
		IS_TRUE0(IS_EQ(v, dual_maxv));
		minv = v;
	}
	return status;
}
#else

/*
Solve minmum problem of linear prgramming naively.
*/
template <class MAT, class T>
UINT SIX<MAT, T>::minm(OUT T & minv, 
					OUT MAT & sol, 
					IN MAT const& tgtf, 
					IN MAT & vc, 
					IN MAT const& eq, 
					IN MAT const& leq, 
					IN INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	MAT neg_tgtf = tgtf;
	neg_tgtf.mul(-1);
	T maxv;
	UINT st = maxm(maxv, sol, neg_tgtf, vc, eq, leq);
	if (st == SIX_SUCC) {
		minv = -maxv;
	}
	return st;
}
#endif


template <class MAT, class T>
bool SIX<MAT, T>::_const_term_is_feasible(MAT & newleq, INT rhs_idx)
{
	IS_TRUE(newleq.get_col_size() - rhs_idx == 1, 
			("multiple const-term is unsupport"));
	for (UINT i = 0; i < newleq.get_row_size(); i++) {
		if (newleq.get(i, rhs_idx) < 0) {
			return false;
		}
	}
	return true;
}


/*
Establish the mapping of variable and relevant inequalities, and try to
find the first feasible solution.
Return ST_SUCC if find a feasible solution, otherwise 
return SIX_NO_PRI_FEASIBLE_SOL.
  
'nvset': Nonbasis variable set
'bvset': Basis variable set
'bv2eqmap, eq2bvmap': mapping from index of basis-variable
	to the index of relevant equality.
'new_rhs_idx': column of constant term when the stage was finished.
*/
template <class MAT, class T>
INT SIX<MAT, T>::_stage_1(OUT MAT & newleq, 
						OUT MAT & newvc, 
						OUT MAT & newtgtf,
						OUT SVECTOR<bool> & nvset,
						OUT SVECTOR<bool> & bvset,
						SVECTOR<INT> & bv2eqmap,
						SVECTOR<INT> & eq2bvmap,
						IN OUT INT & new_rhs_idx)
{
	bool has_pos_coeff_var = false;
	for (UINT i = 0; i < (UINT)new_rhs_idx; i++) {
		if (newtgtf.get(0, i) > 0) {
			has_pos_coeff_var = true;
			break;
		}
	}
	
	//First of all, we are going to find basic feasible solution.
	if (!has_pos_coeff_var || 
		!_const_term_is_feasible(newleq, new_rhs_idx)) {
	//if (!has_pos_coeff_var) {
		/*
		All these coefficient of variables in target function are 
		negative or zero! Thus the possible optimum solution can be 
		gained by set all variable zero. 
		Nonetheless, that solution might be infeasible.
		Hence, the examination for feasibility is indispensable.
		*/
		if (!construct_basic_feasible_solution(newleq, 
												newtgtf, 
												newvc,
												nvset, 
												bvset,
												bv2eqmap, 
												eq2bvmap,
												new_rhs_idx)) {
			return SIX_NO_PRI_FEASIBLE_SOL;
		}
	} else {
		UINT bv_start_idx = new_rhs_idx;

		//A slack transformation.
		_slack(newtgtf, newleq, newvc, new_rhs_idx);
		
		/*
		Initializing basic-variable and non-basic-variable set.
		Mapping from nonbasic-variable to its relevant referrenced
		equalities.
		*/
		UINT i;
		for (i = 0; i < bv_start_idx; i++) {
			bv2eqmap.set(i, INVALID_EQNUM); 
			nvset.set(i, true);
		}

		UINT j = 0;
		for (i = bv_start_idx; i < (UINT)new_rhs_idx; i++, j++) {
			bvset.set(i, true);
			nvset.set(i, false);
			eq2bvmap.set(j, i);			
			bv2eqmap.set(i, j); 
		}
	}
	return SIX_SUCC;
}


/*
Calculate the target-function's value, and derive 
original variable's value from the set of {Nonbasic var} �� {Basic var}.

'rhs_idx': column of const-term of 'slack_sol'
*/
template <class MAT, class T>
void SIX<MAT, T>::calc_final_solution(OUT MAT & sol,
								OUT T & v,
								IN OUT MAT & slack_sol,
								IN INTMAT & vcmap,
								IN MAT const& orignal_tgtf,
								IN INT rhs_idx)
{
	/*
	After normalizing done, new dummy nonbasic variables might be generated,
	recalculate the value to recover the real variables.
	e.g:
		v1 = v1'- v1'', v1 is original variable.
	*/
	if (vcmap.get_row_size() > 0) {
		for (UINT i = 0; i < vcmap.get_row_size(); i++) {
			UINT real_nv_idx = vcmap.get(i, 0);
			UINT dummy_nv_idx1 = vcmap.get(i, 1);	
			UINT dummy_nv_idx2 = vcmap.get(i, 2);	
			IS_TRUE(real_nv_idx < (UINT)rhs_idx && 
					dummy_nv_idx1 < (UINT)rhs_idx && 
					dummy_nv_idx2 < (UINT)rhs_idx, ("illegal vcmap"));
			slack_sol.set(0, real_nv_idx, 
						  slack_sol.get(0, dummy_nv_idx1) - 
						  slack_sol.get(0, dummy_nv_idx2));
		}
	}

	//Recording the real variable's value.
	sol.reinit(1, orignal_tgtf.get_col_size());
	for (UINT i = 0; i < (UINT)m_rhs_idx; i++) {
		sol.set(0, i, slack_sol.get(0, i));
	}
	//Coeff of const-term should be 1.
	for (UINT k = m_rhs_idx; k < (UINT)sol.get_col_size(); k++) {
		sol.set(0, k, 1);
	}

	//Calculating the target function's value.
	v = 0;
	UINT num_of_const_term = sol.get_col_size() - m_rhs_idx;
	IS_TRUE(num_of_const_term == 1, 
			("No yet support const term with multi-columns."));
	for (UINT j = 0; j < (UINT)sol.get_col_size(); j++) {
		v = v + sol.get(0, j) * orignal_tgtf.get(0, j);
	}
}


/*
Two-stages simplex method.

'slack_sol': since the final slack form is involved with some 
	auxiliary variables and slack variables, the final solution 
	resolved recorded temporarily with 'slack_sol' .
*/
template <class MAT, class T>
UINT SIX<MAT, T>::two_stage_method(IN OUT MAT & newleq, 
							IN OUT MAT & newvc, 
							IN OUT MAT & newtgtf,
							IN OUT MAT & slack_sol,
							IN OUT T & maxv,
							IN OUT SVECTOR<bool> & nvset,
							IN OUT SVECTOR<bool> & bvset,
							IN OUT SVECTOR<INT> & bv2eqmap,
							IN OUT SVECTOR<INT> & eq2bvmap,
							IN OUT INT & new_rhs_idx)
{
	UINT status = SIX_SUCC;
	if ((status = _stage_1(newleq, newvc, newtgtf, nvset, bvset, 
							bv2eqmap, eq2bvmap,
							new_rhs_idx)) != SIX_SUCC) {
		return status;
	}
	IS_TRUE0(_const_term_is_feasible(newleq, new_rhs_idx));
	status = _solve_slack_form(newtgtf, newleq,
							newvc, maxv, slack_sol, nvset, bvset,
							bv2eqmap, eq2bvmap, new_rhs_idx);
	return status;
}


/*
Check and alarm if one column all its element be 0 and 'tgtf' 
expected the variable's result. 
If it is the case, the variable that 0-column corresponds to 
has no any constraint, then 'tgtf' could have infinite solution, 
says, the system is unbounded.

Return true if system is unbounded.
e.g:
	There are 3 variable i,j,k, but the second variable j's 
	boundary is empty. And target function is MAX:i+2j+k
		 i  j  k  CST
		-1  0  0  0 
		 1  0  0  99 
		 0  0  1  99 
		-1  0  0  0 
	As aforementioned, the sysmtem is unbound because j 
	can be any integer.
*/
template <class MAT, class T>
bool SIX<MAT, T>::verify_empty_variable_constrain(IN MAT const& tgtf, 
												IN MAT & vc,
												IN MAT const& eq,
												IN MAT const& leq,
												IN INT rhs_idx)
{
	SVECTOR<bool> is_nonzero;
	INT j;
	for (j = 0; j < rhs_idx; j++) {
		if (leq.get_col_size() > 0 && !leq.is_colequ(j, 0)) {
			is_nonzero.set(j, true);
		}
		if (eq.get_col_size() > 0 && !eq.is_colequ(j, 0)) {
			is_nonzero.set(j, true);
		}
	}
	for (j = 0; j < rhs_idx; j++) {		
		if (!is_nonzero.get(j) &&
			tgtf.get(0, j) != 0) {
			IS_TRUE(0, ("%dth variable is unbounded, that will "
						"incur the linear system to be unbound.\n", j));
		}
	}
	return false;
}


/*
Compute linear maximum solution.

Return the result.
	SUCC: Get maximum solution.
	UNBOUND: Target function is unbound.
'sol': optimum feasible solution. 
'vc': variable constraints, one variable one row.
'eq': equalites which the solution should subject to .
'leq': inequalites which the solution should subject to .
'rhs_idx': index number of constant column.
	e.g: Given tgtf as [ax1, bx2, cx3, 100], then rhs_idx is 3.		 

NOTICE: 
	The columns size of 'sol', 'tgtf', 'vc', 'eq', 'leq' must be same.	
*/
template <class MAT, class T>
UINT SIX<MAT, T>::maxm(OUT T & maxv, 
						OUT MAT & sol, 
						IN MAT const& tgtf, 
						IN MAT & vc,
						IN MAT const& eq,
						IN MAT const& leq,
						IN INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));	
	m_rhs_idx = rhs_idx;
	_verify(leq, eq, tgtf, vc, rhs_idx);
	if (verify_empty_variable_constrain(tgtf, vc, eq, leq, m_rhs_idx)) {
		return SIX_UNBOUND;
	}
	
	//Mapping from index of basis-variable to the index of relevant equality.
	MAT newleq, newvc, newtgtf;
	INTMAT vcmap; 
	INT new_rhs_idx = _normalize(newleq, newvc, 
								vcmap, newtgtf, vc, eq, leq, tgtf);
	IS_TRUE(new_rhs_idx > 0, ("at least one variable"));

	T maxv_of_two_stage_iter = 0;
	MAT slack_sol;
	SVECTOR<bool> nvset; //Nonbasis variable set
	SVECTOR<bool> bvset; //Basis variable set	
	SVECTOR<INT> bv2eqmap, eq2bvmap;
	UINT status = two_stage_method(newleq, 
							newvc, 
							newtgtf,
							slack_sol,
							maxv_of_two_stage_iter,
							nvset,
							bvset,
							bv2eqmap,
							eq2bvmap,
							new_rhs_idx);
	maxv = 0;
	if (status == SIX_SUCC) {
		calc_final_solution(sol, maxv, slack_sol, vcmap, tgtf, new_rhs_idx);
		maxv_of_two_stage_iter.reduce(); 
		maxv.reduce();
		sol.reduce();
		IS_TRUE(IS_EQ(maxv_of_two_stage_iter, maxv), ("should be equal"));
	}
	return status;
}


/*
Check if one column of eq/leq that all elements be 0 and 'tgtf' 
expects the result of the variable. If it is the case, the variable 
has not any constraints, and 'tgtf' might have infinite solutions, 
that is the system is unbounded.
	e.g:
		There are 3 variable i,j,k, but the second variable j's 
		boundary is empty. And target function is MAX:i+2j+k
			 i  j  k    CST
			-1  0  0 <= 0 
			 1  0  0 <= 99 
			 0  0  1 <= 99 
			-1  0  0 <= 0 
		As aforementioned, the sysmtem is unbound because j can 
		be any integer.
		Set the coefficient of variable in 'tgtf' to be 0 if no 
		constraints related with it.
*/
template <class MAT, class T>
void SIX<MAT, T>::revise_target_func(IN OUT MAT & tgtf,
									 IN MAT const& eq,
									 IN MAT const& leq,									 
									 IN INT rhs_idx)
{		
	SVECTOR<bool> is_nonzero;
	INT j;
	for (j = 0; j < rhs_idx; j++) {
		if (leq.get_col_size() > 0 && !leq.is_colequ(j, 0)) {
			is_nonzero.set(j, true);
		}
		if (eq.get_col_size() > 0 && !eq.is_colequ(j, 0)) {
			is_nonzero.set(j, true);
		}
	}
	for (j = 0; j < rhs_idx; j++) {		
		if (!is_nonzero.get(j)) {
			tgtf.set(0, j, 0);			
		}
	}
}
//END SIX



//
//START MIP, Mix Integer Programming
//
#define IP_SUCC							0
#define	IP_UNBOUND						1
#define IP_NO_PRI_FEASIBLE_SOL			2
#define IP_NO_BETTER_THAN_BEST_SOL		3

template <class MAT, class T> class MIP : public ELEMT<T> {
	bool m_is_init;
	MAT m_cur_best_sol;
	T m_cur_best_v;
	BMAT * m_allow_rational_indicator;
	INT m_rhs_idx; 
	UINT m_indent;
	UINT m_times;
	UINT _rec_part(OUT T & v,
					OUT MAT & sol,
					IN MAT const& tgtf,
					IN MAT & vc,
					IN MAT const& eq,
					IN MAT const& leq,						
					IN INT rhs_idx,
					IN bool is_max,
					IN bool is_bin,
					IN INTMAT & fork_count);
public:
	MIP();
	~MIP();
	void init();
	void destroy();
	void verify(IN MAT const& leq, 
				IN MAT const& eq, 
				IN MAT const& tgtf, 
				IN MAT const& vc,
				IN INT rhs_idx);
	virtual bool is_satisfying(OUT UINT & row,
								OUT UINT & col, 
								IN MAT & sol,
								bool is_bin);
	virtual UINT minm(OUT T & minv, OUT MAT & res, 
						IN MAT const& tgtf, IN MAT & vc, 
						IN MAT const& eq, IN MAT const& leq,
						IN bool is_bin = false,
						IN BMAT * rational_indicator = NULL,
						IN INT rhs_idx = -1); //Linear minmum solution
	virtual UINT maxm(OUT T & maxv, OUT MAT & res, 
						IN MAT const& tgtf, IN MAT & vc, 
						IN MAT const& eq, IN MAT const& leq,
						IN bool is_bin = false,
						IN BMAT * rational_indicator = NULL,
						IN INT rhs_idx = -1); //Linear maximum solution
	void revise_target_func(IN OUT MAT & tgtf,
							IN MAT const& eq,
							IN MAT const& leq,							
							IN INT rhs_idx);
	virtual FILE * dump_open_file();
	virtual bool dump_prt_indent(FILE * h);
	virtual bool dump_start_six(MAT const& tgtf, 
								MAT & vc, 
								MAT const& eq, 
								MAT const& leq);
	virtual bool dump_end_six(UINT status, T v, MAT & sol);
	virtual bool dump_is_satisfying(MAT & sol);
	virtual bool dump_floor_branch(INT floor);
	virtual bool dump_ceiling_branch(INT ceil);
};


template <class MAT, class T>
MIP<MAT, T>::MIP()
{
	m_is_init = false;
	m_rhs_idx = -1;
	init();
}


template <class MAT, class T>
MIP<MAT, T>::~MIP()
{
  	destroy();
}


template <class MAT, class T>
void MIP<MAT, T>::init()
{
	if (m_is_init) return;
	m_cur_best_sol.init();
	m_cur_best_v = 0;
	m_allow_rational_indicator = NULL;
	m_indent = 0;
	m_times = 0;
	m_is_init = true;
}


template <class MAT, class T>
void MIP<MAT, T>::destroy()
{
	if (!m_is_init) return;
	
	m_rhs_idx = -1;
	m_cur_best_sol.destroy();
	m_cur_best_v = 0;
	m_allow_rational_indicator = NULL;
	m_is_init = false;
}


//Start dump functions
template <class MAT, class T>
FILE * MIP<MAT, T>::dump_open_file()
{
	FILE * h = fopen(SIX_DUMP_NAME, "a+");
	IS_TRUE(h, ("%s create failed!!!", SIX_DUMP_NAME));
	return h;
}


template <class MAT, class T>
bool MIP<MAT, T>::dump_prt_indent(FILE * h)
{
	for (UINT i = 0; i < m_indent; i++) {
		fprintf(h, "  ");	
	}
	return true;
}


template <class MAT, class T>
bool MIP<MAT, T>::dump_start_six(MAT const& tgtf, 
								MAT & vc, 
								MAT const& eq, 
								MAT const& leq)
{
	FILE * h = dump_open_file();
	if (m_times == 0) {
		fprintf(h, "\n");
	}
	dump_prt_indent(h);
	fprintf(h, "TIME=%u...\n", m_times);	
	dump_prt_indent(h);
	fprintf(h, "START SIX...\n");	
	fclose(h);
	return true;
}


template <class MAT, class T>
bool MIP<MAT, T>::dump_end_six(UINT status, T v, MAT & sol)
{
	FILE * h = dump_open_file();
	CHAR buf[32];
	dump_prt_indent(h);
	fprintf(h, "END SIX,status=%d,v=%s,sol is {", status, v.format(buf));
	for (UINT i = 0; i < sol.get_col_size(); i++) {
		fprintf(h, "%s,", sol.get(0, i).format(buf));
	}
	fprintf(h, "}\n");
	fclose(h);
	return true;
}


template <class MAT, class T>
bool MIP<MAT, T>::dump_is_satisfying(MAT & sol)
{
	FILE * h = dump_open_file();
	dump_prt_indent(h);
	fprintf(h, "SOL Satisfying!!! return IP_SUCC.\n");
	fclose(h);
	return true;
}


template <class MAT, class T>
bool MIP<MAT, T>::dump_floor_branch(INT floor)
{
	FILE * h = dump_open_file();
	fprintf(h, "\n");
	dump_prt_indent(h);
	fprintf(h, "ENTER floor(left) branch, floor value=%d\n", floor);
	fclose(h);
	return true;
}


template <class MAT, class T>
bool MIP<MAT, T>::dump_ceiling_branch(INT ceil)
{
	FILE * h = dump_open_file();
	fprintf(h, "\n");
	dump_prt_indent(h);
	fprintf(h, "ENTER ceiling(right) branch, ceiling value=%d\n", ceil);
	fclose(h);
	return true;
}


/*
Verify the legality of input data structures and initialize 
the constant-term column.
*/
template <class MAT, class T>
void MIP<MAT, T>::verify(IN MAT const& leq, 
				IN MAT const& eq, 
				IN MAT const& tgtf, 
				IN MAT const& vc,
				IN INT rhs_idx)
{
	IS_TRUE(rhs_idx == -1 || rhs_idx == (INT)leq.get_col_size() -1, 
			("Parameter is not yet support right now."));
	INT max_cols = -1;
	if (eq.size() != 0 && leq.size() != 0) {
		IS_TRUE(eq.get_col_size() == leq.get_col_size(), 
				("unmatch variables"));
		max_cols = eq.get_col_size();
	} else if (eq.size() != 0) {
		max_cols = eq.get_col_size();
	} else if (leq.size() != 0) {
		max_cols = leq.get_col_size();
	} else {
		IS_TRUE(0, ("no constraints"));
	}
	IS_TRUE(rhs_idx == -1 || rhs_idx == (INT)leq.get_col_size()-1, 
			("unsupport"));

	if (m_rhs_idx == -1) {
		m_rhs_idx = max_cols -1; //Only one const-term.
	} else {
		IS_TRUE(m_rhs_idx < (INT)max_cols && m_rhs_idx >= 1, 
				("out of boundary"));
	}

	UINT num_cols_of_const_term = max_cols - m_rhs_idx; 
	IS_TRUE(num_cols_of_const_term == 1,  
			("No yet support const term with multi-columns."));
	IS_TRUE(tgtf.is_vec() && 
			tgtf.get_col_size() == (UINT)max_cols, 
			("multi target functions"));
	IS_TRUE(vc.get_row_size() == (UINT)m_rhs_idx && 
			vc.get_col_size() == (UINT)max_cols, 
			("unmatch variables constraints"));
	if (m_allow_rational_indicator != NULL) {
		IS_TRUE(m_allow_rational_indicator->get_col_size() == (UINT)max_cols,
				("unmatch variable"));
	}

	/*
	For now, we only permit Variable constraint 'vc' of each 
	variable to be 0, namely only i>=0 is allowed. One should 
	establish constraints i'>=0, and i=i'+3 if i>=3 is required.
	*/
	if (!vc.is_colequ(m_rhs_idx, 0)) {
		IS_TRUE(num_cols_of_const_term == 1, 
				("no yet support const term with multi-columns."));
		IS_TRUE(0, ("variable constraint can only be i>=0"));
	}
	for (INT i = 0; i < m_rhs_idx; i++) {
		if (vc.get(i, i) > 0) {
			IS_TRUE(0, ("coeff of variable must be -1, e.g: -i<=0"));
		}
	}
}


//Return true if 'sol' is satisfying.
template <class MAT, class T>
bool MIP<MAT, T>::is_satisfying(OUT UINT & row,
								OUT UINT & col, 
								IN MAT & sol,
								bool is_bin)
{
	if (m_allow_rational_indicator != NULL) {
		IS_TRUE0(m_allow_rational_indicator->get_row_size() == 
				 sol.get_row_size() &&
				 m_allow_rational_indicator->get_col_size() == 
				 sol.get_col_size());
		for (UINT i = 0; i < sol.get_row_size(); i++) {
			for (UINT j = 0; j < sol.get_col_size(); j++) {
				T v = sol.reduce(i, j);
				if (!m_allow_rational_indicator->get(i, j)) {
					if (!v.is_int()) {
						//Not an integer.
						row = i;
						col = j;
						return false;
					}
					if (is_bin && IS_INEQ(v, T(0)) && IS_INEQ(v, T(1))) {
						//Not an 0-1 integer.
						row = i;
						col = j;
						return false;				
					}
				} //end if
			} //end for
		} //end for
		return true;
	} else if (is_bin) {
		for (UINT i = 0; i < sol.get_row_size(); i++) {
			for (UINT j = 0; j < sol.get_col_size(); j++) {
				T v = sol.reduce(i, j);
				if (IS_INEQ(v, T(0)) && IS_INEQ(v, T(1))) {
					row = i;
					col = j;
					return false;				
				}
			}
		}
		return true;
	}
	return sol.is_imat(&row, &col);
}


template <class MAT, class T>
void MIP<MAT, T>::revise_target_func(IN OUT MAT & tgtf,
									 IN MAT const& eq,
									 IN MAT const& leq,									 
									 IN INT rhs_idx)
{
	SIX<MAT, T> six;
	six.revise_target_func(tgtf, eq, leq, rhs_idx);
}


/*
Recursive subroutine.

'is_max': true refers to solve the maximum problem
'is_bin': true refers to solve the binary(0-1) programming
*/
template <class MAT, class T>
UINT MIP<MAT, T>::_rec_part(OUT T & v,
					OUT MAT & sol,
					IN MAT const& tgtf,
					IN MAT & vc,
					IN MAT const& eq,
					IN MAT const& leq,						
					IN INT rhs_idx,
					IN bool is_max,
					IN bool is_bin,
					IN INTMAT & fork_count)
{
	STATUS status;
	SIX<MAT, T> six;
	six.set_param(m_indent, 10000);
	IS_TRUE0(dump_start_six(tgtf, vc, eq, leq));
	m_times++;	
	if (is_max) {
		status = six.maxm(v, sol, tgtf, vc, eq, leq, rhs_idx);
	} else {
		status = six.minm(v, sol, tgtf, vc, eq, leq, rhs_idx);
	}
	IS_TRUE0(dump_end_six(status, v, sol));
	if (SIX_SUCC != status) {
		switch (status) {
		case SIX_UNBOUND:
			status = IP_UNBOUND;
			break;
		case SIX_NO_PRI_FEASIBLE_SOL:
			status = IP_NO_PRI_FEASIBLE_SOL;
			break;
		case SIX_OPTIMAL_IS_INFEASIBLE:
			status = IP_NO_PRI_FEASIBLE_SOL;
			break;
		case SIX_TIME_OUT:
		default:
			IS_TRUE0(0);
		}
		return status;
	}
	
	//Query integer solution by cutting convex with hyper-plane.
	UINT row, col;
	if (is_satisfying(row, col, sol, is_bin)) {
		IS_TRUE0(dump_is_satisfying(sol));
		return IP_SUCC;
	}
	if (m_cur_best_sol.size() != 0) {
		//Find a solution, do check if it involved rational value.
		if (is_max) {
			if (IS_LE(v, m_cur_best_v)) {
				return IP_NO_BETTER_THAN_BEST_SOL;		
			}
		} else {
			if (IS_GE(v, m_cur_best_v)) {
				return IP_NO_BETTER_THAN_BEST_SOL;		
			}
		}
	}
	if (fork_count.get(0, col) >= 1) {
		/*e.g: Avoid the cyclic recusive computing.
		1th, the fork point is 0,
				221/3, 63, 0, 3, 72,    65      
		2th, the fork point is 5,
				73,    63, 0, 3, 214/3, 65
		3th, the fork point is 0,
				221/3, 63, 0, 3, 72,    65      
		...
		*/
		return IP_NO_PRI_FEASIBLE_SOL;
	}
	fork_count.set(0, col, fork_count.get(0, col)+1);

	INT sol_floor = 0;
	INT sol_ceil = 0;
	MAT new_constraint(1, tgtf.get_col_size());

	//Solving the floor part.
	MAT tleq = leq;
	MAT teq = eq;	
	if (is_bin) {
		//Append equations
		sol_floor = 0;
		sol_ceil = 1;
		new_constraint.set(0, col, 1);
		new_constraint.set(0, m_rhs_idx, sol_floor);
		teq.grow_row(new_constraint, 0, new_constraint.get_row_size() - 1);
	} else {
		//Append LT constraints
		IS_TRUE(row == 0, ("only support unique solution"));
		sol_floor = sol.get(row, col).typecast2int();
		sol_ceil = sol_floor + 1;
		new_constraint.set(0, col, 1);
		new_constraint.set(0, m_rhs_idx, sol_floor);
		tleq.grow_row(new_constraint, 0, new_constraint.get_row_size() - 1);
	}	
	
	MAT tmp_sol;
	T tmpv;
	m_indent++;
	IS_TRUE0(dump_floor_branch(sol_floor));
	if (IP_SUCC == (status = _rec_part(v, sol, tgtf, vc, teq, 
									   tleq, rhs_idx, is_max, 
									   is_bin, fork_count))) {
		tmp_sol = sol;
		tmpv = v;
		if (is_max) {
			if (m_cur_best_sol.size() == 0 || m_cur_best_v < v) {
				m_cur_best_sol = sol;
				m_cur_best_v = v;
			}	
		} else {
			if (m_cur_best_sol.size() == 0 || m_cur_best_v > v) {
				m_cur_best_sol = sol;
				m_cur_best_v = v;
			}	
		}		
	}
	
	//Solving the ceil part.
	tleq = leq;
	teq = eq;
	if (is_bin) {
		//Append equations
		new_constraint.zero();
		new_constraint.set(0, col, 1);
		new_constraint.set(0, m_rhs_idx, sol_ceil);
		teq.grow_row(new_constraint, 0, new_constraint.get_row_size() - 1);
	} else {
		//Append LT constraints
		new_constraint.zero();
		new_constraint.set(0, col, -1);
		new_constraint.set(0, m_rhs_idx, -sol_ceil);		
		tleq.grow_row(new_constraint, 0, new_constraint.get_row_size() - 1);
	}
	IS_TRUE0(dump_ceiling_branch(sol_ceil));

	if (IP_SUCC == (status = _rec_part(v, sol, tgtf, vc, teq, 
									   tleq, rhs_idx, is_max, 
									   is_bin, fork_count))) {
		if (is_max) {
			//Maximum problem
			if (tmp_sol.size() != 0) {
				if (tmpv > v) {
					v = tmpv;
					sol = tmp_sol;
				}
			}
			if (m_cur_best_sol.size() == 0 || m_cur_best_v < v) {
				m_cur_best_sol = sol;
				m_cur_best_v = v;
			}
		} else { 
			//Minimum problem
			if (tmp_sol.size() != 0) {
				if (tmpv < v) {
					v = tmpv;
					sol = tmp_sol;
				}
			}
			if (m_cur_best_sol.size() == 0 || m_cur_best_v > v) {
				m_cur_best_sol = sol;
				m_cur_best_v = v;
			}		
		}
		m_indent--;
		return IP_SUCC;
	} else if (tmp_sol.size() != 0) {
		v = tmpv;
		sol = tmp_sol;
		if (is_max) {
			if (m_cur_best_sol.size() == 0 || m_cur_best_v < v) {
				m_cur_best_sol = sol;
				m_cur_best_v = v;
			}
		} else {
			if (m_cur_best_sol.size() == 0 || m_cur_best_v > v) {
				m_cur_best_sol = sol;
				m_cur_best_v = v;
			}		
		}
		m_indent--;
		return IP_SUCC;
	}
	m_indent--;
	return status;
}


/*
Compute maximum solution of mixed integer programming.

Return the result.
	SUCC: Get maximum solution.
	UNBOUND: Target function is unbound.
'sol': optimum feasible solution. 
'vc': variable constraints, one variable one row.
'eq': equalites which the solution should subject to .
'leq': inequalites which the solution should subject to .
'is_bin': true refers to solve the binary(0-1) programming, 
		else to solve integer programming.
'rational_indicator': if it is not NULL, TRUE element means 
		the solution permits to be rational.
	e.g: If rational_indicator(0, j) is TRUE, the element j 
		 of solution could be rational.
'rhs_idx': index number of constant column.
	e.g: Given tgtf as [ax1, bx2, cx3, 100], then rhs_idx is 3.		 

NOTICE: 
	The columns size of 'sol', 'tgtf', 'vc', 'eq', 'leq' must be same.
*/
template <class MAT, class T>
UINT MIP<MAT, T>::maxm(OUT T & maxv, 
				OUT MAT & sol, 
				IN MAT const& tgtf, 
				IN MAT & vc,
				IN MAT const& eq, 
				IN MAT const& leq,
				IN bool is_bin,
				IN BMAT * rational_indicator,
				IN INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));	
	m_allow_rational_indicator = rational_indicator;
	verify(leq, eq, tgtf, vc, rhs_idx);
	m_cur_best_sol.clean();
	m_cur_best_v = 0;
	m_times = 0;
	m_indent = 0;
	INTMAT fork_count(1, tgtf.get_col_size());
	return _rec_part(maxv, sol, tgtf, vc, eq, leq, 
					 rhs_idx, true, is_bin, fork_count);
}


/*
Solve the minimum solution of mixed integer programming.

Return the result.
	SUCC: Get minimum solution.
	UNBOUND: Target function is unbound.
'sol': optimum feasible solution. 
'vc': variable constraints, one variable one row.
'eq': equalites which the solution should subject to .
'leq': inequalites which the solution should subject to .
'is_bin': true refers to solve the binary(0-1) programming, 
		else to solve integer programming.
'rational_indicator': if it is not NULL, TRUE element means 
		the solution permits to be rational.
	e.g: If rational_indicator(0, j) is TRUE, the element j 
		of solution could be rational.
'rhs_idx': index number of constant column.
	e.g: Given tgtf as [ax1, bx2, cx3, 100], then rhs_idx is 3.		 

NOTICE: 
	The columns size of 'sol', 'tgtf', 'vc', 'eq', 'leq' must be same.	
*/
template <class MAT, class T>
UINT MIP<MAT, T>::minm(OUT T & minv,
				OUT MAT & sol,
				IN MAT const& tgtf,
				IN MAT & vc,
				IN MAT const& eq,
				IN MAT const& leq,
				IN bool is_bin,
				IN BMAT * rational_indicator,
				IN INT rhs_idx)
{
	IS_TRUE(m_is_init, ("not yet initialize"));
	m_allow_rational_indicator = rational_indicator;
	verify(leq, eq, tgtf, vc, rhs_idx);
	m_cur_best_sol.clean();
	m_cur_best_v = 0;
	m_times = 0;
	m_indent = 0;
	INTMAT fork_count(1, tgtf.get_col_size());
	return _rec_part(minv, sol, tgtf, vc, eq, leq, 
					 rhs_idx, false, is_bin, fork_count);
}
#endif

