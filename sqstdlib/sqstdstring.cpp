/* see copyright notice in squirrel.h */
#include <squirrel.h>
#include <sqstdstring.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#ifdef SQUNICODE
#define scstrchr wcschr
#define scsnprintf wsnprintf
#define scatoi _wtoi
#define scstrtok wcstok
#else
#define scstrchr strchr
#define scsnprintf snprintf
#define scatoi atoi
#define scstrtok strtok
#endif
#define MAX_FORMAT_LEN	20
#define MAX_WFORMAT_LEN	3
#define ADDITIONAL_FORMAT_SPACE (100*sizeof(SQChar))

static SQInteger validate_format(HSQUIRRELVM v, SQChar *fmt, const SQChar *src, SQInteger n,SQInteger &width)
{
	SQChar swidth[MAX_WFORMAT_LEN];
	SQInteger wc = 0;
	SQInteger start = n;
	fmt[0] = '%';
	while (scstrchr(_SC("-+ #0"), src[n])) n++;
	while (scisdigit(src[n])) {
		swidth[wc] = src[n];
		n++;
		wc++;
		if(wc>=MAX_WFORMAT_LEN)
			return sq_throwerror(v,_SC("width format too long"));
	}
	swidth[wc] = '\0';
	if(wc > 0) {
		width = scatoi(swidth);
	}
	else
		width = 0;
	if (src[n] == '.') {
	    n++;

		wc = 0;
		while (scisdigit(src[n])) {
			swidth[wc] = src[n];
			n++;
			wc++;
			if(wc>=MAX_WFORMAT_LEN)
				return sq_throwerror(v,_SC("precision format too long"));
		}
		swidth[wc] = '\0';
		if(wc > 0) {
			width += scatoi(swidth);
		}
	}
	if (n-start > MAX_FORMAT_LEN )
		return sq_throwerror(v,_SC("format too long"));
	memcpy(&fmt[1],&src[start],((n-start)+1)*sizeof(SQChar));
	fmt[(n-start)+2] = '\0';
	return n;
}

SQRESULT sqstd_format(HSQUIRRELVM v,SQInteger nformatstringidx,SQInteger *outlen,SQChar **output)
{
	const SQChar *format;
	SQChar *dest;
	SQChar fmt[MAX_FORMAT_LEN];
	sq_getstring(v,nformatstringidx,&format);
	SQInteger allocated = (sq_getsize(v,nformatstringidx)+2)*sizeof(SQChar);
	dest = sq_getscratchpad(v,allocated);
	SQInteger n = 0,i = 0, nparam = nformatstringidx+1, w = 0;
	while(format[n] != '\0') {
		if(format[n] != '%') {
			assert(i < allocated);
			dest[i++] = format[n];
			n++;
		}
		else if(format[n+1] == '%') { //handles %%
				dest[i++] = '%';
				n += 2;
		}
		else {
			n++;
			if( nparam > sq_gettop(v) )
				return sq_throwerror(v,_SC("not enough paramters for the given format string"));
			n = validate_format(v,fmt,format,n,w);
			if(n < 0) return -1;
			SQInteger addlen = 0;
			SQInteger valtype = 0;
			const SQChar *ts;
			SQInteger ti;
			SQFloat tf;
			SQInteger fc = format[n];
			switch(fc) {
            case 'q':
			case 's':
				if(SQ_FAILED(sq_getstring(v,nparam,&ts)))
					return sq_throwerror(v,_SC("string expected for the specified format"));
                if(fc == 'q'){
                    addlen = 2; //quotes before and after
                    SQInteger size = sq_getsize(v,nparam);
                    SQChar *ts2 = (SQChar*)ts;
                      while (size--) {
                        ++addlen;
                        if (*ts2 == '\r' && *(ts2+1) == '\n' ) {
                          ++addlen;
                          ++ts2; //eat \r and output only \n
                        }
                        else if (*ts2 == '"' || *ts2 == '\\' || *ts2 == '\n') {
                          ++addlen;
                        }
                        else if (*ts2 == '\0' || iscntrl(uchar(*ts2))) {
                          SQChar buff[10];
                          addlen += scsprintf(buff, "\\x%x", (int)uchar(*ts2))-1; //-1 because we already added the original char to the sum
                        }
                        ++ts2;
                      }

                      ts2 = (SQChar*)i; //save the i position using pointer as integer
                      i += addlen;
                      allocated += addlen;
                      dest = sq_getscratchpad(v,allocated);
                      size = sq_getsize(v,nparam);

                      ts2 = &dest[(int)ts2]; //use saved i position saved on pointer as integer
                      *ts2++ = '"';
                      while (size--) {
                        if (*ts == '\r' && *(ts+1) == '\n' ) {
                          ++ts; //eat \r and output only \n
                        }
                        if (*ts == '"' || *ts == '\\') {
                            *ts2++ = '\\';
                            *ts2++ = *ts;
                        }
                        else if (*ts == '\n') {
                            *ts2++ = '\\';
                            *ts2++ = 'n';
                        }
                        else if (*ts == '\0' || iscntrl(uchar(*ts))) {
                          SQChar buff[10];
                          int iw;
                          iw = scsprintf(buff, "\\x%x", (int)uchar(*ts));
                          for(int i=0; i< iw; ++i) *ts2++ = buff[i];
                        }
                        else
                            *ts2++ = *ts;
                        ++ts;
                      }
                      *ts2++ = '"';

                      ++n;
                      ++nparam;
                      continue;
                }
                else
                {
                    addlen = (sq_getsize(v,nparam)*sizeof(SQChar))+((w+1)*sizeof(SQChar));
                    valtype = 's';
                }
				break;
			case 'i': case 'd': case 'o': case 'u':  case 'x':  case 'X':
#ifdef _SQ64
				{
				size_t flen = scstrlen(fmt);
				SQInteger fpos = flen - 1;
				SQChar f = fmt[fpos];
				SQChar *prec = (SQChar *)_PRINT_INT_PREC;
				while(*prec != _SC('\0')) {
					fmt[fpos++] = *prec++;
				}
				fmt[fpos++] = f;
				fmt[fpos++] = _SC('\0');
				}
#endif
			case 'c':
				if(SQ_FAILED(sq_getinteger(v,nparam,&ti)))
					return sq_throwerror(v,_SC("integer expected for the specified format"));
				addlen = (ADDITIONAL_FORMAT_SPACE)+((w+1)*sizeof(SQChar));
				valtype = 'i';
				break;
			case 'f': case 'g': case 'G': case 'e':  case 'E':
				if(SQ_FAILED(sq_getfloat(v,nparam,&tf)))
					return sq_throwerror(v,_SC("float expected for the specified format"));
				addlen = (ADDITIONAL_FORMAT_SPACE)+((w+1)*sizeof(SQChar));
				valtype = 'f';
				break;
			default:
				return sq_throwerror(v,_SC("invalid format"));
			}
			n++;
			allocated += addlen + sizeof(SQChar);
			dest = sq_getscratchpad(v,allocated);
			switch(valtype) {
			case 's': i += scsprintf(&dest[i],fmt,ts); break;
			case 'i': i += scsprintf(&dest[i],fmt,ti); break;
			case 'f': i += scsprintf(&dest[i],fmt,tf); break;
			};
			nparam ++;
		}
	}
	*outlen = i;
	dest[i] = '\0';
	*output = dest;
	return SQ_OK;
}

static SQInteger _string_format(HSQUIRRELVM v)
{
	SQChar *dest = NULL;
	SQInteger length = 0;
	if(SQ_FAILED(sqstd_format(v,2,&length,&dest)))
		return -1;
	sq_pushstring(v,dest,length);
	return 1;
}

static SQInteger _string_printf(HSQUIRRELVM v)
{
	SQChar *dest = NULL;
	SQInteger length = 0;
	SQPRINTFUNCTION sqprint = sq_getprintfunc(v);
    if(sqprint){
        if(SQ_FAILED(sqstd_format(v,2,&length,&dest)))
            return -1;
        sqprint(v,_SC("%s"),dest);
        sq_pushinteger(v, length);
        return 1;
    }
	return 0;
}

#define SETUP_REX(v) \
	SQRex *self = NULL; \
	sq_getinstanceup(v,1,(SQUserPointer *)&self,0);

static SQInteger _rexobj_releasehook(SQUserPointer p, SQInteger size, HSQUIRRELVM v)
{
	SQRex *self = ((SQRex *)p);
	sqstd_rex_free(self);
	return 1;
}

static SQInteger _regexp_match(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str;
	sq_getstring(v,2,&str);
	if(sqstd_rex_match(self,str) == SQTrue)
	{
		sq_pushbool(v,SQTrue);
		return 1;
	}
	sq_pushbool(v,SQFalse);
	return 1;
}

static SQInteger _regexp_gmatch(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str;
	SQInteger str_size;
	sq_getstring(v,2,&str);
	str_size = sq_getsize(v, 2);
	const SQChar *begin,*end;
	while(sqstd_rex_searchrange(self,str, str+str_size,&begin,&end)){
	    SQInteger n = sqstd_rex_getsubexpcount(self);
	    SQRexMatch match;
	    sq_pushroottable(v); //this
	    SQInteger i = 0;
	    for(;i < n; i++) {
            sqstd_rex_getsubexp(self,i,&match);
            if(i > 0){ //skip whole match
                sq_pushstring(v, match.begin, match.len);
            }
		}
		i = sq_call(v, n, SQFalse, SQTrue);
		if(i < 0) return i;
		str_size -= end-str;
		str = end;
	}
	sq_pushbool(v,SQFalse);
	return 1;
}

#include "sqstdblobimpl.h"
static SQInteger _regexp_gsub(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str;
	SQInteger str_size;
	sq_getstring(v,2,&str);
	str_size = sq_getsize(v, 2);
	const SQChar *begin,*end;
	SQBlob blob(0,8192);
	SQObjectType ptype = sq_gettype(v, 3);
    const SQChar *replacement;
    SQInteger replacement_size;
	while(sqstd_rex_searchrange(self,str, str+str_size,&begin,&end)){
	    blob.Write(str, begin-str);
	    SQInteger n = sqstd_rex_getsubexpcount(self);
	    SQRexMatch match;
        SQInteger i = 0;
	    switch(ptype){
	        case OT_CLOSURE:{
                sq_pushroottable(v); //this
                for(;i < n; i++) {
                    sqstd_rex_getsubexp(self,i,&match);
                    if(i > 0){ //skip whole match
                        sq_pushstring(v, match.begin, match.len);
                    }
                }
                i = sq_call(v, n, SQTrue, SQTrue);
                if(i < 0) return i;
                if(sq_gettype(v, -1) == OT_STRING){
                    const SQChar *svalue;
                    sq_getstring(v, -1, &svalue);
                    blob.Write(svalue, sq_getsize(v, -1));
                }
                sq_poptop(v);
	        }
	        break;
	        case OT_ARRAY:{
                for(;i < n; i++) {
                    sqstd_rex_getsubexp(self,i,&match);
                    if(i > 0){ //skip whole match
                        sq_pushinteger(v, i-1);
                        if(SQ_SUCCEEDED(sq_get(v, 3)) &&
                                SQ_SUCCEEDED(sq_getstr_and_size(v, -1, &replacement, &replacement_size))){
                            blob.Write(replacement, replacement_size);
                            sq_pop(v, 1); //remove value
                        }
                    }
                }
	        }
	        break;
	        case OT_TABLE:{
                for(;i < n; i++) {
                    sqstd_rex_getsubexp(self,i,&match);
                    if(i > 0){ //skip whole match
                        sq_pushstring(v, match.begin, match.len);
                        if(SQ_SUCCEEDED(sq_get(v, 3)) &&
                                SQ_SUCCEEDED(sq_getstr_and_size(v, -1, &replacement, &replacement_size))){
                            blob.Write(replacement, replacement_size);
                            sq_pop(v, 1); //remove value
                        }
                    }
                }
	        }
	        break;
	        default:
                return sq_throwerror(v, _SC("gsub only works with closure, array, table for replacement"));
	    }
		str_size -= end-str;
		str = end;
	}
    if(str_size) blob.Write(str, str_size);
	sq_pushstring(v, (const SQChar *)blob.GetBuf(), blob.Len());
	return 1;
}

static void _addrexmatch(HSQUIRRELVM v,const SQChar *str,const SQChar *begin,const SQChar *end)
{
	sq_newtable(v);
	sq_pushstring(v,_SC("begin"),-1);
	sq_pushinteger(v,begin - str);
	sq_rawset(v,-3);
	sq_pushstring(v,_SC("end"),-1);
	sq_pushinteger(v,end - str);
	sq_rawset(v,-3);
}

static SQInteger _regexp_search(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str,*begin,*end;
	SQInteger start = 0;
	sq_getstring(v,2,&str);
	if(sq_gettop(v) > 2) sq_getinteger(v,3,&start);
	if(sqstd_rex_search(self,str+start,&begin,&end) == SQTrue) {
		_addrexmatch(v,str,begin,end);
		return 1;
	}
	return 0;
}

static SQInteger _regexp_capture(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str,*begin,*end;
	SQInteger start = 0;
	sq_getstring(v,2,&str);
	if(sq_gettop(v) > 2) sq_getinteger(v,3,&start);
	if(sqstd_rex_search(self,str+start,&begin,&end) == SQTrue) {
		SQInteger n = sqstd_rex_getsubexpcount(self);
		SQRexMatch match;
		sq_newarray(v,0);
		for(SQInteger i = 0;i < n; i++) {
			sqstd_rex_getsubexp(self,i,&match);
			if(match.len > 0)
				_addrexmatch(v,str,match.begin,match.begin+match.len);
			else
				_addrexmatch(v,str,str,str); //empty match
			sq_arrayappend(v,-2);
		}
		return 1;
	}
	return 0;
}

static SQInteger _regexp_xcapture(HSQUIRRELVM v)
{
	SETUP_REX(v);
	const SQChar *str,*begin,*end;
	SQInteger start = 0;
	sq_getstring(v,2,&str);
	if(sq_gettop(v) > 2) sq_getinteger(v,3,&start);
	if(sqstd_rex_search(self,str+start,&begin,&end) == SQTrue) {
	    sq_pushbool(v, SQTrue);
		return 1;
	}
	return 0;
}

static SQInteger _regexp_getxcapture(HSQUIRRELVM v)
{
	SETUP_REX(v);
	SQInteger n, start;
	const SQChar *str;
	sq_getstring(v,2,&str);
	sq_getinteger(v,3,&n);
	SQRexMatch match;
    sqstd_rex_getsubexp(self,n,&match);
    if(match.len > 0){
        start = match.begin-str;
        sq_pushinteger(v, start);
	    sq_arrayappend(v,-2);
        sq_pushinteger(v, start+match.len);
	    sq_arrayappend(v,-2);
	    sq_pushbool(v, SQTrue);
		return 1;
    }
	return 0;
}

static SQInteger _regexp_subexpcount(HSQUIRRELVM v)
{
	SETUP_REX(v);
	sq_pushinteger(v,sqstd_rex_getsubexpcount(self));
	return 1;
}

static SQInteger _regexp_constructor(HSQUIRRELVM v)
{
	const SQChar *error,*pattern;
	sq_getstring(v,2,&pattern);
	SQRex *rex = sqstd_rex_compile(pattern,&error);
	if(!rex) return sq_throwerror(v,error);
	sq_setinstanceup(v,1,rex);
	sq_setreleasehook(v,1,_rexobj_releasehook);
	return 0;
}

static SQInteger _regexp__typeof(HSQUIRRELVM v)
{
	sq_pushstring(v,_SC("regexp"),-1);
	return 1;
}

#define _DECL_REX_FUNC(name,nparams,pmask) {_SC(#name),_regexp_##name,nparams,pmask}
static SQRegFunction rexobj_funcs[]={
	_DECL_REX_FUNC(constructor,2,_SC(".s")),
	_DECL_REX_FUNC(search,-2,_SC("xsn")),
	_DECL_REX_FUNC(match,2,_SC("xs")),
	_DECL_REX_FUNC(gmatch,3,_SC("xsc")),
	_DECL_REX_FUNC(gsub,3,_SC("xs c|a|t")),
	_DECL_REX_FUNC(capture,-2,_SC("xsn")),
	_DECL_REX_FUNC(xcapture,-2,_SC("xsn")),
	_DECL_REX_FUNC(getxcapture,4,_SC("xsna")),
	_DECL_REX_FUNC(subexpcount,1,_SC("x")),
	_DECL_REX_FUNC(_typeof,1,_SC("x")),
	{0,0}
};
#undef _DECL_REX_FUNC

#define _DECL_FUNC(name,nparams,pmask) {_SC(#name),_string_##name,nparams,pmask}
static SQRegFunction stringlib_funcs[]={
	_DECL_FUNC(printf,-2,_SC(".s")),
	_DECL_FUNC(format,-2,_SC(".s")),
	{0,0}
};
#undef _DECL_FUNC


SQInteger sqstd_register_stringlib(HSQUIRRELVM v)
{
	sq_pushstring(v,_SC("regexp"),-1);
	sq_newclass(v,SQFalse);
	SQInteger i = 0;
	while(rexobj_funcs[i].name != 0) {
		SQRegFunction &f = rexobj_funcs[i];
		sq_pushstring(v,f.name,-1);
		sq_newclosure(v,f.f,0);
		sq_setparamscheck(v,f.nparamscheck,f.typemask);
		sq_setnativeclosurename(v,-1,f.name);
		sq_newslot(v,-3,SQFalse);
		i++;
	}
	sq_newslot(v,-3,SQFalse);

	i = 0;
	while(stringlib_funcs[i].name!=0)
	{
		sq_pushstring(v,stringlib_funcs[i].name,-1);
		sq_newclosure(v,stringlib_funcs[i].f,0);
		sq_setparamscheck(v,stringlib_funcs[i].nparamscheck,stringlib_funcs[i].typemask);
		sq_setnativeclosurename(v,-1,stringlib_funcs[i].name);
		sq_newslot(v,-3,SQFalse);
		i++;
	}
	return 1;
}
