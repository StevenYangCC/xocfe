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
#ifndef __FLTY_H__
#define __FLTY_H__

/*
//The precision of 'double' is too high to
//some operation of those value that approaching infinitesimal.
//e.g: when the value is 0.00000000000000066613381477509392,
//it equals to 0 in actually.
*/
#define PRECISION_TYPE	double
#define INFINITESIMAL 0.00000000000000001
class FLTY {
	friend FLTY zerolinz(FLTY const& a);
	friend bool operator == (FLTY const& a, FLTY const& b);
	friend bool operator != (FLTY const& a, FLTY const& b);
	friend bool operator < (FLTY const& a, FLTY const& b);
	friend bool operator <= (FLTY const& a, FLTY const& b);
	friend bool operator > (FLTY const& a, FLTY const& b);
	friend bool operator >= (FLTY const& a, FLTY const& b);
	friend FLTY operator * (FLTY const& a, FLTY const& b);
	friend FLTY operator / (FLTY const& a, FLTY const& b);
	friend FLTY operator + (FLTY const& a, FLTY const& b);
	friend FLTY operator - (FLTY const& a, FLTY const& b);
	friend FLTY operator - (FLTY a);
protected:
	PRECISION_TYPE m_f;
public:
	FLTY()
	{
		m_f = PRECISION_TYPE(0);
	}

	FLTY(FLTY const& f)
	{
		//Sometimes, r need not to initialize always.
		//IS_TRUE(r.m_den != 0, ("denominator is 0!"));
		m_f = f.m_f;
	}

	FLTY(PRECISION_TYPE f)
	{
		m_f = f;
	}

	FLTY & operator = (FLTY const& a)
	{
		m_f = a.m_f;
		return *this;
	}

	//Calculate the floor boundary.
	INT typecast2int()
	{
		return (INT)m_f;
	}

	bool is_int();
	PRECISION_TYPE f() const {return m_f;}
	PRECISION_TYPE& f() {return m_f;}
	void reduce() {}
	CHAR * format(CHAR * buf);
};


//Exported Functions
bool operator == (FLTY const& a, FLTY const& b);
bool operator != (FLTY const& a, FLTY const& b);
inline bool operator != (FLTY const& a, FLTY const& b) { return !(a == b); }
bool operator < (FLTY const& a, FLTY const& b);
bool operator <= (FLTY const& a, FLTY const& b);
bool operator > (FLTY const& a, FLTY const& b);
bool operator >= (FLTY const& a, FLTY const& b);
FLTY operator * (FLTY const& a, FLTY const& b);
FLTY operator / (FLTY const& a, FLTY const& b);
FLTY operator + (FLTY const& a, FLTY const& b);
FLTY operator - (FLTY const& a, FLTY const& b);
FLTY operator - (FLTY a);
PRECISION_TYPE integralize(PRECISION_TYPE const& a);
FLTY zerolinz(FLTY const& a);
#endif
