#include <map>

#include "decoder.h"
#include "wbxml.h"

std::map<STokenId, STokenData> g_mapTokenList;

bool operator < (const STokenId &p_soLeft, const STokenId &p_soRight)
{
	if (p_soLeft.m_ui8CodePage < p_soRight.m_ui8CodePage)
		return true;
	if (p_soLeft.m_ui8CodePage > p_soRight.m_ui8CodePage)
		return false;
	if (p_soLeft.m_strType < p_soRight.m_strType)
		return true;
	if (p_soLeft.m_strType > p_soRight.m_strType)
		return false;
	if (p_soLeft.m_ui8Token < p_soRight.m_ui8Token)
		return true;
	return false;
}

STokenData * GetTokenData (u_int8 p_uiPageindex, const char *p_pszTokenType, u_int8 p_ui8Token);
int ParseElement (CFileReader &p_coFileReader, const char *p_pszStrTbl, int p_iIndent);
int ParseAttribute (const char *p_pszExpectedTokenType, CFileReader &p_coFileReader, const char *p_pszStrTbl, int p_iIndent);

int InitDecoderDict ()
{
	int iRetVal = 0;
	STokenId soTokenId;
	STokenData soTokenData;

	for (int i = 0; i < sizeof (g_msoTokenDescr) / sizeof (*g_msoTokenDescr); i++) {
		soTokenId.m_ui8CodePage = g_msoTokenDescr[i].m_ui8CodePage;
		soTokenId.m_strType = g_msoTokenDescr[i].m_strType;
		if (soTokenId.m_strType == "element")
			soTokenId.m_ui8Token = g_msoTokenDescr[i].m_soToken.m_uiToken;
		else
			soTokenId.m_ui8Token = g_msoTokenDescr[i].m_soToken.m_ui8Value;
		soTokenData.m_strName = g_msoTokenDescr[i].m_strTokenName;
		soTokenData.m_strValue = g_msoTokenDescr[i].m_strTokenValue;
		g_mapTokenList.insert (std::make_pair (soTokenId, soTokenData));
	}

	return iRetVal;
}

int Decode (CFileReader &p_coFileReader)
{
	int iRetVal = 0;
	int iFnRes;
	int iDataLen;
	unsigned long long ullPublicid = 0;
	unsigned long long ullCharset = 0;
	unsigned long long ullStrTblLen = 0;
	char *pszStrTbl = NULL;

	do {
		/* считываем версию документа */
		SWBXMLVersion soVersion;
		iDataLen = sizeof (soVersion);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&soVersion, iDataLen);
		/* проверяем результат выполнения операции */
		if (iFnRes) {
			iRetVal = iFnRes;
			break;
		}
		/* проверяем достаточно ли нам полученных данных */
		if (iDataLen != sizeof (soVersion)) {
			iRetVal = ENODATA;
			break;
		}

		printf ("document version: %d.%d\n", soVersion.m_uiMajor + 1, soVersion.m_uiMinor);

		/* считываем public id */
		SWBXMLPublicId soPublicid;
		iDataLen = sizeof (soPublicid);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&soPublicid, iDataLen);
		/* проверяем результат выполнения операции */
		if (iFnRes) {
			iRetVal = iFnRes;
			break;
		}
		/* проверяем достаточно ли нам полученных данных */
		if (iDataLen != sizeof (soVersion)) {
			iRetVal = ENODATA;
			break;
		}

		if (0 != soPublicid.m_uiZero) {
			ullPublicid = ub_u_int32_toLongLong (soPublicid.m_mbPublicId, p_coFileReader, iFnRes);
			if (iFnRes) {
				iRetVal = iFnRes;
				break;
			}
		}

		printf ("public id: %llu; description: %s\n", ullPublicid, ullPublicid > sizeof (g_msoPublicidInfo) / sizeof (SPublicidInfo) ? "invalid value" : g_msoPublicidInfo[ullPublicid].m_strName.data ());

		/* считываем кодировку */
		SWBXMLCharset soCharset;
		iDataLen = sizeof (soCharset);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&soCharset, iDataLen);
		/* проверяем результат выполнения операции */
		if (iFnRes) {
			iRetVal = iFnRes;
			break;
		}
		/* проверяем достаточно ли нам полученных данных */
		if (iDataLen != sizeof (soCharset)) {
			iRetVal = ENODATA;
			break;
		}

		if (0 != soCharset.m_uiZero) {
			ullCharset = ub_u_int32_toLongLong (soCharset.m_mbCharset, p_coFileReader, iFnRes);
			/* проверяем результат выполнения операции */
			if (iFnRes) {
				iRetVal = iFnRes;
				break;
			}
		}

		printf ("charset: %llu\n", ullCharset);

		/* считываем таблицу строк */
		SWBXMLStringtbl soStringtbl;
		iDataLen = sizeof (soStringtbl);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&soStringtbl, iDataLen);
		/* проверяем результат выполнения операции */
		if (iFnRes) {
			iRetVal = iFnRes;
			break;
		}
		/* проверяем достаточно ли нам полученных данных */
		if (iDataLen != sizeof (soStringtbl)) {
			iRetVal = ENODATA;
			break;
		}

		ullStrTblLen = ub_u_int32_toLongLong (soStringtbl.m_mbLength, p_coFileReader, iFnRes);
		/* проверяем результат выполнения операции */
		if (iFnRes) {
			iRetVal = iFnRes;
			break;
		}

		/* если таблица строк не пустая */
		if (ullStrTblLen) {
			pszStrTbl = new char[ullStrTblLen];
			iDataLen = (int)ullStrTblLen;
			iFnRes = p_coFileReader.ReadData ((unsigned char*)pszStrTbl, iDataLen);
			/* проверяем результат выполнения операции */
			if (iFnRes) {
				iRetVal = iFnRes;
				break;
			}
			/* проверяем достаточно ли нам полученных данных */
			if (iDataLen != (int)ullStrTblLen) {
				iRetVal = ENODATA;
				break;
			}
		}

		printf ("string table length: %llu\n", ullStrTblLen);

		/* парсин тела документа */
		u_int8 ui8CodePage = 0;
		iRetVal = ParseElement (p_coFileReader, pszStrTbl, 0);
	} while (0);

	if (pszStrTbl) {
		delete[] pszStrTbl;
		pszStrTbl = NULL;
	}

	return iRetVal;
}

static char mcIndent[] = "                                                             ";
#define INDENT(i) &mcIndent[sizeof (mcIndent) - 1 > i ? sizeof (mcIndent) - 1 - i : 0]

STokenData * GetTokenData (u_int8 p_uiPageindex, const char *p_pszTokenType, u_int8 p_ui8Token)
{
	STokenId soTokenId;
	STokenData *psoRetVal = NULL;
	std::map<STokenId, STokenData>::iterator iterList;

	do {
		/* ищем среди глобальных токенов */
		soTokenId.m_ui8CodePage = -1;
		soTokenId.m_strType = "element";
		soTokenId.m_ui8Token = p_ui8Token;
		iterList = g_mapTokenList.find (soTokenId);
		if (iterList != g_mapTokenList.end ()) {
			psoRetVal = &iterList->second;
			break;
		}
		/* если найти среди глобальных токенов не удалось */
		soTokenId.m_ui8CodePage = p_uiPageindex;
		soTokenId.m_strType = p_pszTokenType;
		soTokenId.m_ui8Token = p_ui8Token;
		iterList = g_mapTokenList.find (soTokenId);

		if (iterList != g_mapTokenList.end ())
			psoRetVal = &iterList->second;
	} while (0);

	return psoRetVal;
}

int ParseElement (CFileReader &p_coFileReader, const char *p_pszStrTbl, int p_iIndent)
{
	int iRetVal = 0;
	int iFnRes;
	SWBXMLToken soToken;
	const char *pszTokenName;
	int iDataLen;
	STokenData *psoTokenData;
	static u_int8 ui8PageIndex = 0;

	/* считываем токен */
	iDataLen = sizeof (soToken);
	iFnRes = p_coFileReader.ReadData ((unsigned char*)&soToken, iDataLen);
	if (iRetVal) {
		iRetVal = iFnRes;
		return iRetVal;
	}

	/* SWITCH_PAGE */
	if (0x00 == soToken.m_uiToken) {
		u_int8 ui8Value;
		/* считываем новый page index */
		iDataLen = sizeof (ui8Value);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&ui8Value, iDataLen);
		if (iRetVal) {
			iRetVal = iFnRes;
			return iRetVal;
		}
		ui8PageIndex = ui8Value;
		/* считываем следующий токен */
		iDataLen = sizeof (soToken);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&soToken, iDataLen);
		if (iRetVal) {
			iRetVal = iFnRes;
			return iRetVal;
		}
	}

	/* END token */
	if (0x01 == soToken.m_uiToken) {
		return 1;
	}

	if (0x04 == soToken.m_uiToken) {
		/* LITERAL */
		unsigned long long ullInd;
		mb_u_int32 mbInd;
		iDataLen = sizeof (mbInd);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&mbInd, iDataLen);
		if (iFnRes) {
			iRetVal = iFnRes;
			return iRetVal;
		}
		ullInd = ub_u_int32_toLongLong (mbInd, p_coFileReader, iFnRes);
		if (iFnRes) {
			iRetVal = iFnRes;
			return iRetVal;
		}
		pszTokenName = &p_pszStrTbl[ullInd];
	} else {
		psoTokenData = GetTokenData (ui8PageIndex, "element", soToken.m_uiToken);
		if (psoTokenData) {
			pszTokenName = psoTokenData->m_strName.c_str ();
		} else {
			pszTokenName = "unknown token";
			printf ("%s'unknown token':%#04x@%#04x", INDENT (p_iIndent), soToken.m_uiToken, ui8PageIndex);
		}
	}
	printf ("%s<%s", INDENT (p_iIndent), pszTokenName);

	/* обрабатываем атрибуты */
	if (soToken.m_bAttr) {
		ParseAttribute ("attribute", p_coFileReader, p_pszStrTbl, p_iIndent + 1);
	}

	/* обрабатываем контент */
	if (soToken.m_bContent) {
		printf (">\n");
		while (0 == ParseElement (p_coFileReader, p_pszStrTbl, p_iIndent + 1));
		printf ("%s</%s>\n", INDENT (p_iIndent), pszTokenName);
	} else {
		printf ("/>\n", INDENT (p_iIndent));
	}

	return iRetVal;
}

int ParseAttribute (const char *p_pszExpectedTokenType, CFileReader &p_coFileReader, const char *p_pszStrTbl, int p_iIndent)
{
	int iRetVal = 0;
	int iDataLen;
	int iFnRes;
	u_int8 ui8Attr;
	STokenData *psoTokenData;
	static u_int8 ui8Pageindex = 0;

	do {
		iDataLen = sizeof (ui8Attr);
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&ui8Attr, iDataLen);
		if (iFnRes) {
			iRetVal = iFnRes;
			break;
		}
		if (sizeof (ui8Attr) != iDataLen) {
			iRetVal = ENODATA;
			break;
		}
		/* SWITCH_PAGE */
		if (0x00 == ui8Attr) {
			/* считываем новый page index */
			iDataLen = sizeof (ui8Attr);
			iFnRes = p_coFileReader.ReadData ((unsigned char*)&ui8Attr, iDataLen);
			if (iRetVal) {
				iRetVal = iFnRes;
				return iRetVal;
			}
			ui8Pageindex = ui8Attr;
			/* считываем следующий токен */
			iDataLen = sizeof (ui8Attr);
			iFnRes = p_coFileReader.ReadData ((unsigned char*)&ui8Attr, iDataLen);
			if (iFnRes) {
				iRetVal = iFnRes;
				break;
			}
			if (sizeof (ui8Attr) != iDataLen) {
				iRetVal = ENODATA;
				break;
			}
		}
		switch (ui8Attr) {
			unsigned char c;
		case 0x01:	/* END */
			if (ui8Attr == 0x1) {
				iRetVal = 1;
			}
			break;
		case 0x03: /* STR_I */
		case 0x40: /* EXT_I_*/
		case 0x41: /* EXT_I_*/
		case 0x42: /* EXT_I_*/
			iDataLen = 1;
			printf ("=\"");
			do {
				if (0 == p_coFileReader.ReadData (&c, iDataLen)) {
					if (iDataLen == 1) {
						if ('\0' == c)
							break;
						printf ("%c", c);
					} else {
						iRetVal = -1;
						break;
					}
				} else {
					iRetVal = -1;
				}
			} while (c);
			printf ("\"");
			break;
		case 0x04: /* LITERAL */
		case 0x80: /* EXT_T_*/
		case 0x81: /* EXT_T_*/
		case 0x82: /* EXT_T_*/
			printf (" ");
			unsigned long long ullInd;
			mb_u_int32 mbInd;
			iDataLen = sizeof (mbInd);
			iFnRes = p_coFileReader.ReadData ((unsigned char*)&mbInd, iDataLen);
			if (iFnRes) {
				iRetVal = iFnRes;
				return iRetVal;
			}
			ullInd = ub_u_int32_toLongLong (mbInd, p_coFileReader, iFnRes);
			if (iFnRes) {
				iRetVal = iFnRes;
				return iRetVal;
			}
			printf (&p_pszStrTbl[ullInd]);
			p_pszExpectedTokenType = "value";
			break;
		default:
			if (strcmp (p_pszExpectedTokenType, "value"))
				printf (" ");
			psoTokenData = GetTokenData (ui8Pageindex, p_pszExpectedTokenType, ui8Attr);
			if (NULL == psoTokenData) {
				printf ("%#04x@%#04x", ui8Attr, ui8Pageindex);
			} else {
				if (0 == strcmp (p_pszExpectedTokenType, "value"))
					printf ("=\"");
				printf ("%s", psoTokenData->m_strName.c_str ());
				if (0 == strcmp (p_pszExpectedTokenType, "value"))
					printf ("\"");
			}
			/* проверяем есть ли у атрибута значение */
			if (psoTokenData && psoTokenData->m_strValue.length ()) {
				printf ("=");
				printf ("\"%s\"", psoTokenData->m_strValue.c_str ());
			} else {
				p_pszExpectedTokenType = "value";
			}
			break;
		}
		if (iRetVal) {
			break;
		}

		ParseAttribute (p_pszExpectedTokenType, p_coFileReader, p_pszStrTbl, p_iIndent);
	} while (0);

	return iRetVal;
}
