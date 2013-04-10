/*
 *
  Copyright (c) Dialogic(R), 2010

 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "/usr/include/asterisk/channel.h"

#ifdef CC_AST_HAS_VERSION_10_0 /* { */

#define CC_FORMAT_ALAW      (1ULL << 3)
#define CC_FORMAT_G722      (1ULL << 12)
#define CC_FORMAT_G723_1    (1ULL << 0)
#define CC_FORMAT_G726      (1ULL << 11)
#define CC_FORMAT_G729A     (1ULL << 8)
#define CC_FORMAT_GSM       (1ULL << 1)
#define CC_FORMAT_ILBC      (1ULL << 10)
#define CC_FORMAT_SIREN14   (1ULL << 14)
#define CC_FORMAT_SIREN7    (1ULL << 13)
#define CC_FORMAT_SLINEAR   (1ULL << 6)
#define CC_FORMAT_SLINEAR16 (1ULL << 15)
#define CC_FORMAT_ULAW      (1ULL << 2)

static inline enum ast_format_id ccCodec2AstCodec(int ccCodec)
{
#if 0
	unsigned int ret;

	switch (ccCodec) {
		case CC_FORMAT_ALAW:
			ret = AST_FORMAT_ALAW;
			break;
		case CC_FORMAT_G722:
			ret = AST_FORMAT_G722;
			break;
		case CC_FORMAT_G723_1:
			ret = AST_FORMAT_G723_1;
			break;
		case CC_FORMAT_G726:
			ret = AST_FORMAT_G726;
			break;
		case CC_FORMAT_G729A:
			ret = AST_FORMAT_G729A;
			break;
		case CC_FORMAT_GSM:
			ret = AST_FORMAT_GSM;
			break;
		case CC_FORMAT_ILBC:
			ret = AST_FORMAT_ILBC;
			break;
		case CC_FORMAT_SIREN14:
			ret = AST_FORMAT_SIREN14;
			break;
		case CC_FORMAT_SIREN7:
			ret = AST_FORMAT_SIREN7;
			break;
		case CC_FORMAT_SLINEAR:
			ret = AST_FORMAT_SLINEAR;
			break;
		case CC_FORMAT_SLINEAR16:
			ret = AST_FORMAT_SLINEAR16;
			break;
		case CC_FORMAT_ULAW:
			ret = AST_FORMAT_ULAW;
			break;

		default:
			ret = 0;
			break;
	}

	return ret;
#else
	return (ast_format_id_from_old_bitfield(ccCodec));
#endif
}

static inline const char* cc_getformatname(int ccCodec)
{
	enum ast_format_id id = ccCodec2AstCodec (ccCodec);
	struct ast_format fmt;

	ast_format_clear(&fmt);
	ast_format_set(&fmt, id, 0);

	return ast_getformatname(&fmt);
}

static inline void cc_add_formats(struct ast_format_cap *fmts, unsigned int divaFormats)
{
	int i;

	for (i = 0; i < 32; i++) {
		unsigned int ccCodec = (1U << i);

		if ((divaFormats & ccCodec) != 0) {
			enum ast_format_id id = ccCodec2AstCodec (ccCodec);
			struct ast_format fmt;

			ast_format_clear(&fmt);
			ast_format_set(&fmt, id, 0);
			ast_format_cap_add(fmts, &fmt);
		}
	}
}

static inline int cc_set_best_codec(struct ast_channel *a)
{
	struct ast_format bestCodec;

	ast_format_clear(&bestCodec);

	#ifdef CC_AST_HAS_VERSION_11_0
	if (ast_best_codec(ast_channel_nativeformats(a), &bestCodec) == NULL) {
		/*
			Fallback to aLaw
			*/
		ast_format_set(&bestCodec, CC_FORMAT_ALAW, 0);
	}

	ast_format_copy(ast_channel_rawreadformat(a),  &bestCodec);
	ast_format_copy(ast_channel_readformat(a),     &bestCodec);
	ast_format_copy(ast_channel_rawwriteformat(a), &bestCodec);
	ast_format_copy(ast_channel_writeformat(a),    &bestCodec);
	#else
	if (ast_best_codec(a->nativeformats, &bestCodec) == NULL) {
		/*
			Fallback to aLaw
			*/
		ast_format_set(&bestCodec, CC_FORMAT_ALAW, 0);
	}

	ast_format_copy(&a->rawreadformat,  &bestCodec);
	ast_format_copy(&a->readformat,     &bestCodec);
	ast_format_copy(&a->rawwriteformat, &bestCodec);
	ast_format_copy(&a->writeformat,    &bestCodec);
	#endif

	return (int)ast_format_to_old_bitfield(&bestCodec);
}

static inline void cc_set_read_format(struct ast_channel* a, int ccCodec)
{
	struct ast_format ccFmt;

	ast_format_clear(&ccFmt);
	ast_format_set(&ccFmt, ccCodec, 0);
	ast_set_read_format(a, &ccFmt);
}

static inline void cc_set_write_format(struct ast_channel* a, int ccCodec)
{
	struct ast_format ccFmt;

	ast_format_clear(&ccFmt);
	ast_format_set(&ccFmt, ccCodec, 0);
	ast_set_write_format(a, &ccFmt);
}

#define cc_parse_allow_disallow(__prefs__, __capability__, __value__, __allowing__, __cap__) do{\
	ast_parse_allow_disallow(__prefs__, __cap__, __value__, __allowing__); \
	*(__capability__) = (int)ast_format_cap_to_old_bitfield(__cap__); }while(0)

#define cc_get_formats_as_bits(__a__) (int)ast_format_cap_to_old_bitfield(__a__)

static inline int cc_get_best_codec_as_bits(int src)
{
	struct ast_format_cap *dst = ast_format_cap_alloc_nolock();
	int ret = 0;

	if (dst != 0) {
		struct ast_format bestCodec;

		ast_format_cap_from_old_bitfield(dst, src);
		ast_format_clear(&bestCodec);
		if (ast_best_codec(dst, &bestCodec) != NULL) {
			ret = (int)ast_format_to_old_bitfield(&bestCodec);
		}
		ast_format_cap_destroy(dst);
	}

	return (ret);
}

#define cc_set_read_format(__a__, __b__)  ast_set_read_format_by_id(__a__, ccCodec2AstCodec (__b__))

#define cc_set_write_format(__a__, __b__) ast_set_write_format_by_id(__a__, ccCodec2AstCodec (__b__))

static inline int cc_getformatbyname(const char* value)
{
	struct ast_format fmt;
	int ret = 0;

	if (ast_getformatbyname(value, &fmt) != NULL) {
		ret = (int)ast_format_to_old_bitfield(&fmt);
	}

	return (ret);
}

#else /* } { */

#define CC_FORMAT_ALAW      AST_FORMAT_ALAW
#ifdef AST_FORMAT_G722
#define CC_FORMAT_G722      AST_FORMAT_G722
#endif
#define CC_FORMAT_G723_1    AST_FORMAT_G723_1
#define CC_FORMAT_G726      AST_FORMAT_G726
#define CC_FORMAT_G729A     AST_FORMAT_G729A
#define CC_FORMAT_GSM       AST_FORMAT_GSM
#define CC_FORMAT_ILBC      AST_FORMAT_ILBC
#ifdef AST_FORMAT_SIREN14
#define CC_FORMAT_SIREN14   AST_FORMAT_SIREN14
#endif
#ifdef AST_FORMAT_SIREN7
#define CC_FORMAT_SIREN7    AST_FORMAT_SIREN7
#endif
#ifdef AST_FORMAT_SLINEAR
#define CC_FORMAT_SLINEAR   AST_FORMAT_SLINEAR
#endif
#ifdef AST_FORMAT_SLINEAR16
#define CC_FORMAT_SLINEAR16 AST_FORMAT_SLINEAR16
#endif
#define CC_FORMAT_ULAW      AST_FORMAT_ULAW


#define cc_getformatname(__x__) ast_getformatname((__x__))
#define cc_add_formats(__x__,__y__) do {(__x__)=(__y__);}while(0)

static inline int cc_set_best_codec(struct ast_channel *a)
{
	int fmt = ast_best_codec(a->nativeformats);

	a->readformat     = fmt;
	a->writeformat    = fmt;
	a->rawreadformat  = fmt;
	a->rawwriteformat = fmt;

	return fmt;
}

#define cc_set_read_format(__a__, __b__) ast_set_read_format(__a__, __b__)
#define cc_set_write_format(__a__, __b__) ast_set_write_format(__a__, __b__)
#define cc_parse_allow_disallow(__a__, __b__, __c__, __d__, __e__) ast_parse_allow_disallow(__a__, __b__, __c__, __d__)
#define cc_get_formats_as_bits(__a__) (__a__)
#define cc_get_best_codec_as_bits(__a__) ast_best_codec(__a__)
#define cc_getformatbyname(__a__) ast_getformatbyname(__a__)
#endif /* } */
