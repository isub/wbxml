#include <expat.h>
#include <errno.h>
#include <map>
#include <vector>

#include "encoder.h"
#include "utils/filewriter/filewriter.h"

extern SPublicidInfo g_msoPublicidInfo[];
extern STokenDescr g_msoTokenDescr[];

struct SHeader {
	SWBXMLVersion m_soVersion;
	SWBXMLPublicId m_soPublicId;
	SWBXMLCharset m_soCharSet;
	std::vector<std::string> m_vectStrTbl;
	SHeader () { m_soVersion.m_uiMajor = 0; m_soVersion.m_uiMinor = 3; m_soPublicId.m_uiZero = 0; m_soCharSet.m_mbCharset.m_uiValue = 106; }
};

struct SBody {
	std::string m_strValue;
	std::string m_strName;
	std::string m_strType;
	SBody *m_psoParent;
	SBody *m_psoAttrList;
	SBody *m_psoAttrLast;
	SBody *m_psoChildList;
	SBody *m_psoChildLast;
	SBody *m_psoPrec;
	SBody *m_psoNext;
	SBody () { m_psoParent = NULL; m_psoAttrList = NULL; m_psoAttrLast = NULL;  m_psoChildList = NULL;  m_psoChildLast = NULL;  m_psoNext = NULL; m_psoPrec = NULL; }
};

struct SDoc {
	std::basic_string<unsigned char> m_strBinHedr;
	std::basic_string<unsigned char> m_strBinBody;
	SBody *m_psoBodyFirst;
	SBody *m_psoBodyLast;
	SHeader m_soHdr;
	void AddElement (const char *p_pszName);
	void AddAttr (const char *p_pszName, const char *p_pszValue);
	void CloseElement ();
	SDoc () { m_psoBodyFirst = NULL; m_psoBodyLast = NULL; }
};

void StartElementHandler (void *userData, const XML_Char *name, const XML_Char **atts);
void EndElementHandler (void *userData, const XML_Char *name);
void XmlDeclHandler (void *userData, const XML_Char *version, const XML_Char *encoding, int standalone);
void StartDoctypeDeclHandler (void *userData, const XML_Char *doctypeName, const XML_Char *sysid, const XML_Char *pubid, int has_internal_subset);
void EndDoctypeDeclHandler (void *userData);

void Tokenize (SDoc &p_soDoc);
void TokenizeBody (SBody *p_soBody, std::basic_string<unsigned char> &p_strBin, SDoc &p_soDoc);
void TokenizeElement (SBody *p_soBody, std::basic_string<unsigned char> &p_strBin, SDoc &p_soDoc);
void TokinizeAttribute (SBody *p_soBody, std::basic_string<unsigned char> &p_strBin, SDoc &p_soDoc);
void CreateLiteral (SBody *p_soBody, SDoc &p_soDoc, std::basic_string<mb_u_int32> &p_strInd);

int Encode (CFileReader &p_coFileReader)
{
	int iRetVal = 0;
	XML_Parser psoParser;

	psoParser = XML_ParserCreate ("UTF-8");
	if (NULL == psoParser) {
		iRetVal = ENOMEM;
		return iRetVal;
	}

	SDoc soDoc;

	/* регистрация обработчика данных */
	XML_SetElementHandler (psoParser, StartElementHandler, EndElementHandler);
	XML_SetXmlDeclHandler (psoParser, XmlDeclHandler);
	XML_SetDoctypeDeclHandler (psoParser, StartDoctypeDeclHandler, EndDoctypeDeclHandler);
	XML_SetUserData (psoParser, &soDoc);

	/* парсинг данных */
	char mcBuf[256];
	int iDataLen;
	int iIsFinal = 0;
	do {
		iDataLen = sizeof (mcBuf);
		if (p_coFileReader.ReadData ((unsigned char*)mcBuf, iDataLen))
			iIsFinal = 1;
		XML_Parse (psoParser, mcBuf, iDataLen, iIsFinal);
		if (iIsFinal)
			break;
	} while (1);

	Tokenize (soDoc);

	if (psoParser) {
		XML_ParserFree (psoParser);
		psoParser = NULL;
	}

	return iRetVal;
}

void StartElementHandler (void *userData, const XML_Char *name, const XML_Char **atts)
{
	((SDoc*)userData)->AddElement (name);
	for (int i = 0; atts[i]; i++, i++) {
		((SDoc*)userData)->AddAttr (atts[i], atts[i + 1] ? atts[i + 1] : "");
	}
}

void EndElementHandler (void *userData, const XML_Char *name)
{
	((SDoc*)userData)->CloseElement ();
}

void XmlDeclHandler (void *userData, const XML_Char *version, const XML_Char *encoding, int standalone)
{
	char *pszEnd;

	((SDoc*)userData)->m_soHdr.m_soVersion.m_uiMajor = strtol (version, &pszEnd, 10) - 1;
	if (pszEnd && pszEnd != version) {
		pszEnd++;
		((SDoc*)userData)->m_soHdr.m_soVersion.m_uiMinor = strtol (pszEnd, &pszEnd, 10);
	}
}

void StartDoctypeDeclHandler (void *userData, const XML_Char *doctypeName, const XML_Char *sysid, const XML_Char *pubid, int has_internal_subset)
{
	for (int i = 0; i < sizeof (g_msoPublicidInfo) / sizeof (*g_msoPublicidInfo); i++) {
		if (0 == g_msoPublicidInfo[i].m_strName.compare (pubid)) {
			((SDoc*)userData)->m_soHdr.m_soPublicId.m_mbPublicId.m_uiValue = i;
			break;
		}
	}
}

void EndDoctypeDeclHandler (void *userData)
{
}

void SDoc::AddElement (const char *p_pszName)
{
	SBody *psoTmp = NULL;

	psoTmp = new SBody;
	psoTmp->m_strType = "element";
	psoTmp->m_strName = p_pszName;
	psoTmp->m_psoParent = m_psoBodyLast;

	if (NULL == m_psoBodyFirst) {
		m_psoBodyFirst = psoTmp;
	}
	if (m_psoBodyLast) {
		psoTmp->m_psoParent = m_psoBodyLast;
		if (NULL == m_psoBodyLast->m_psoChildList) {
			m_psoBodyLast->m_psoChildList = psoTmp;
		} else {
			m_psoBodyLast->m_psoChildLast->m_psoNext = psoTmp;
			psoTmp->m_psoPrec = m_psoBodyLast->m_psoChildLast;
		}
		m_psoBodyLast->m_psoChildLast = psoTmp;
	}
	m_psoBodyLast = psoTmp;
}

void SDoc::AddAttr (const char *p_pszName, const char *p_pszValue)
{
	SBody *psoTmp;

	psoTmp = new SBody;
	psoTmp->m_strType = "attribute";
	psoTmp->m_strName = p_pszName;
	if(p_pszValue)
		psoTmp->m_strValue = p_pszValue;

	/* недопустимая ошибка */
	if (NULL == m_psoBodyLast)
		return;
	/* если список атрибутов еще не задан */
	if (NULL == m_psoBodyLast->m_psoAttrList) {
		m_psoBodyLast->m_psoAttrList = psoTmp;
		m_psoBodyLast->m_psoAttrLast = psoTmp;
	} else {
		psoTmp->m_psoPrec = m_psoBodyLast->m_psoAttrLast;
		m_psoBodyLast->m_psoAttrLast->m_psoNext = psoTmp;
	}
	m_psoBodyLast->m_psoAttrLast = psoTmp;
	psoTmp->m_psoParent = m_psoBodyLast;
}

void SDoc::CloseElement ()
{
	if (m_psoBodyLast->m_psoParent)
		m_psoBodyLast = m_psoBodyLast->m_psoParent;
}

struct STokenInd {
	std::string m_strType;
	std::string m_strName;
	std::string m_strValue;
};

bool operator < (const STokenInd &p_soLeft, const STokenInd &p_soRight)
{
	if (p_soLeft.m_strType < p_soRight.m_strType)
		return true;
	if (p_soLeft.m_strType > p_soRight.m_strType)
		return false;
	if (p_soLeft.m_strName < p_soRight.m_strName)
		return true;
	if (p_soLeft.m_strName > p_soRight.m_strName)
		return false;
	if (p_soLeft.m_strValue < p_soRight.m_strValue)
		return true;
	return false;
}

static std::map<STokenInd, std::map<u_int8, u_int8>> g_mapTokenizer;

void InitEncoderDict ()
{
	STokenInd soTokenInd;
	std::map<STokenInd, std::map<u_int8, u_int8>>::iterator iterTokenInd;

	for (int i = 0; i < sizeof (g_msoTokenDescr) / sizeof (*g_msoTokenDescr); i++) {
		soTokenInd.m_strType = g_msoTokenDescr[i].m_strType;
		soTokenInd.m_strName = g_msoTokenDescr[i].m_strTokenName;
		soTokenInd.m_strValue = g_msoTokenDescr[i].m_strTokenValue;
		iterTokenInd = g_mapTokenizer.find (soTokenInd);
		if (iterTokenInd == g_mapTokenizer.end ()) {
			std::map<u_int8, u_int8> mapTmp;
			mapTmp.insert (std::make_pair (g_msoTokenDescr[i].m_ui8CodePage, g_msoTokenDescr[i].m_soToken.m_ui8Value));
			g_mapTokenizer.insert (std::make_pair (soTokenInd, mapTmp));
		} else {
			iterTokenInd->second.insert (std::make_pair (g_msoTokenDescr[i].m_ui8CodePage, g_msoTokenDescr[i].m_soToken.m_ui8Value));
		}
	}
}

void Tokenize (SDoc &p_soDoc)
{
	int iFnRes;
	CFileWriter coFileWriter;

	iFnRes = coFileWriter.Init ();
	if (iFnRes)
		return;
	iFnRes = coFileWriter.CreateOutputFile ("out.wbxml");
	if (iFnRes)
		return;

	/* формируем версию */
	p_soDoc.m_strBinHedr.append ((unsigned char*)&p_soDoc.m_soHdr.m_soVersion, sizeof (p_soDoc.m_soHdr.m_soVersion));
	/* формируем public_id */
	p_soDoc.m_strBinHedr.append ((unsigned char*)&p_soDoc.m_soHdr.m_soPublicId, sizeof (p_soDoc.m_soHdr.m_soPublicId));
	/* формируем charset */
	p_soDoc.m_strBinHedr.append ((unsigned char*)&p_soDoc.m_soHdr.m_soCharSet, sizeof (p_soDoc.m_soHdr.m_soCharSet));
	/* формируем body */
	u_int8 ui8CodePage = 0;
	TokenizeBody (p_soDoc.m_psoBodyFirst, p_soDoc.m_strBinBody, p_soDoc);
	/* формируем strtbl */
	std::string strStrTbl;
	for (unsigned int i = 0; i < p_soDoc.m_soHdr.m_vectStrTbl.size (); i++) {
		strStrTbl.append (p_soDoc.m_soHdr.m_vectStrTbl[i].data (), p_soDoc.m_soHdr.m_vectStrTbl[i].length () + 1);
	}
	std::basic_string<mb_u_int32> strStrLen;
	LongLong_toub_u_int32 (strStrTbl.length (), strStrLen);
	p_soDoc.m_strBinHedr.append ((unsigned char*)strStrLen.data (), strStrLen.length());
	if (strStrTbl.length ())
		p_soDoc.m_strBinHedr.append ((unsigned char*)strStrTbl.data (), strStrTbl.length ());

	coFileWriter.WriteData (p_soDoc.m_strBinHedr.data (), p_soDoc.m_strBinHedr.size ());
	coFileWriter.WriteData (p_soDoc.m_strBinBody.data (), p_soDoc.m_strBinBody.size ());

	coFileWriter.Finalise ();
}

void TokenizeBody (SBody *p_soBody, std::basic_string<unsigned char> &p_strBin, SDoc &p_soDoc)
{
	SBody *psoBody;
	STokenInd soTokenInd;
	std::map<STokenInd, std::map<u_int8, u_int8>>::iterator iterTokenizer;
	std::map<u_int8, u_int8>::iterator iterToken;

	psoBody = p_soBody;
	while (psoBody) {
		TokenizeElement (psoBody, p_strBin, p_soDoc);
		if (psoBody->m_psoAttrList) {
			TokinizeAttribute (psoBody->m_psoAttrList, p_strBin, p_soDoc);
			p_strBin.append ((unsigned char*)"\x01", 1);
		}
		if (psoBody->m_psoChildList) {
			TokenizeElement (psoBody->m_psoChildList, p_strBin, p_soDoc);
			p_strBin.append ((unsigned char*)"\x01", 1);
		}
		psoBody = psoBody->m_psoNext;
	}
}

void TokenizeElement (SBody *p_psoBody, std::basic_string<unsigned char> &p_strBin, SDoc &p_soDoc)
{
	static u_int8 ui8CodePage = 0;
	SBody *psoBody;
	STokenInd soTokenInd;
	std::map<STokenInd, std::map<u_int8, u_int8>>::iterator iterTokenizer;
	std::map<u_int8, u_int8>::iterator iterToken;
	SWBXMLToken soToken = { 0 };

	psoBody = p_psoBody;
	while (psoBody) {
		/* инициализируем структуру для поиска токена */
		soTokenInd.m_strType = psoBody->m_strType;
		soTokenInd.m_strName = psoBody->m_strName;
		soTokenInd.m_strValue = psoBody->m_strValue;
		/* запрашиваем нобходимый токен */
		iterTokenizer = g_mapTokenizer.find (soTokenInd);
		/* если токен не найден */
		if (iterTokenizer == g_mapTokenizer.end () && soTokenInd.m_strValue.length ()) {
			soTokenInd.m_strValue = "";
			/* ищем токен повторно для пустого значения */
			iterTokenizer = g_mapTokenizer.find (soTokenInd);
		}
		/* задаем флаг атрибутов */
		if (psoBody->m_psoAttrList)
			soToken.m_bAttr = 1;
		/* задаем флаг контента */
		if (psoBody->m_psoChildList)
			soToken.m_bContent = 1;
		/* если токен так и не найден */
		if (iterTokenizer == g_mapTokenizer.end ()) {
			/* LITERAL */
			soToken.m_uiToken = 0x04;
			p_strBin.append (&soToken.m_ui8Value, sizeof (soToken));
			/* определяем индекс литерала */
			std::basic_string<mb_u_int32> strInd;
			CreateLiteral (p_psoBody, p_soDoc, strInd);
			for (unsigned int i = 0; i < strInd.length (); i++)
				p_strBin.append (&(strInd[i].m_uiValue), 1);
		} else {
			/* формируем токен */
			iterToken = iterTokenizer->second.find (ui8CodePage);
			/* если значение, соответствующее текущей кодовой странице, найдено */
			if (iterToken == iterTokenizer->second.end ()) {
				iterToken = iterTokenizer->second.begin ();
				ui8CodePage = iterToken->first;
				/* меняем code page элемента */
				p_strBin.append ((unsigned char*)"\x00", 1);
				p_strBin.append ((unsigned char*)&(iterToken->first), 1);
			}
			soToken.m_uiToken = iterToken->second;
			/* формируем токен */
			p_strBin.append (&soToken.m_ui8Value, sizeof (soToken));
		}
		if (psoBody->m_psoAttrList) {
			TokinizeAttribute (psoBody->m_psoAttrList, p_strBin, p_soDoc);
			p_strBin.append ((unsigned char*)"\x01", 1);
		}
		if (psoBody->m_psoChildList) {
			TokenizeElement (psoBody->m_psoChildList, p_strBin, p_soDoc);
			p_strBin.append ((unsigned char*)"\x01", 1);
		}
		psoBody = psoBody->m_psoNext;
	}
}

void TokinizeAttribute (SBody *p_psoBody, std::basic_string<unsigned char> &p_strBin, SDoc &p_soDoc)
{
	static u_int8 ui8CodePage = 0;
	SBody *psoBody;
	STokenInd soTokenInd;
	std::map<STokenInd, std::map<u_int8, u_int8>>::iterator iterTokenizer;
	std::map<u_int8, u_int8>::iterator iterToken;
	SWBXMLToken soToken = { 0 };

	psoBody = p_psoBody;
	while (psoBody) {
		/* инициализируем структуру для поиска токена */
		soTokenInd.m_strType = psoBody->m_strType;
		soTokenInd.m_strName = psoBody->m_strName;
		soTokenInd.m_strValue = psoBody->m_strValue;
		/* запрашиваем нобходимый токен */
		iterTokenizer = g_mapTokenizer.find (soTokenInd);
		/* если токен не найден */
		if (iterTokenizer == g_mapTokenizer.end () && soTokenInd.m_strValue.length ()) {
			soTokenInd.m_strValue = "";
			/* ищем токен повторно для пустого значения */
			iterTokenizer = g_mapTokenizer.find (soTokenInd);
		}
		/* если токен так и не найден */
		if (iterTokenizer == g_mapTokenizer.end ()) {
			/* LITERAL */
			soToken.m_uiToken = 0x04;
			p_strBin.append (&soToken.m_ui8Value, sizeof (soToken));
			/* определяем индекс литерала */
			std::basic_string<mb_u_int32> strInd;
			CreateLiteral (p_psoBody, p_soDoc, strInd);
			for (unsigned int i = 0; i < strInd.length (); i++)
				p_strBin.append (&(strInd[i].m_uiValue), 1);
		} else {
			/* формируем токен */
			iterToken = iterTokenizer->second.find (ui8CodePage);
			/* если значение, соответствующее текущей кодовой странице, найдено */
			if (iterToken == iterTokenizer->second.end ()) {
				iterToken = iterTokenizer->second.begin ();
				ui8CodePage = iterToken->first;
				p_strBin.append ((unsigned char*)"\x00", 1);
				p_strBin.append ((unsigned char*)&(iterToken->first), 1);
			}
			soToken.m_ui8Value = iterToken->second;
			/* формируем токен */
			p_strBin.append (&soToken.m_ui8Value, sizeof (soToken));
		}
		/* если значение не токенизировано */
		if (psoBody->m_strValue.length () && 0 == soTokenInd.m_strValue.length ()) {
			/* ищем токенизированное значение */
			soTokenInd.m_strType = "value";
			soTokenInd.m_strName = psoBody->m_strValue;
			soTokenInd.m_strValue = "";
			iterTokenizer = g_mapTokenizer.find (soTokenInd);
			/* если подходящий токен не найден */
			if (iterTokenizer == g_mapTokenizer.end ()) {
				p_strBin.append ((unsigned char*)"\x03", 1);
				p_strBin.append ((unsigned char*)psoBody->m_strValue.c_str (), psoBody->m_strValue.length () + 1);
			} else {
				iterToken = iterTokenizer->second.find (ui8CodePage);
				if (iterToken == iterTokenizer->second.end ()) {
					iterToken = iterTokenizer->second.begin ();
					ui8CodePage = iterToken->first;
					p_strBin.append ((unsigned char*)"\x00", 1);
					p_strBin.append ((unsigned char*)&(iterToken->first), 1);
				}
				soToken.m_ui8Value = iterToken->second;
				p_strBin.append (&soToken.m_ui8Value, 1);
			}
		}
		psoBody = psoBody->m_psoNext;
	}
}

void CreateLiteral (SBody *p_soBody, SDoc &p_soDoc, std::basic_string<mb_u_int32> &p_strInd)
{
	unsigned long long ullInd = 0;
	bool bItFound = false;

	/* сначала ищем строку среди ранее созаданных */
	for (unsigned int i = 0; i < p_soDoc.m_soHdr.m_vectStrTbl.size (); i++) {
		if (0 == p_soDoc.m_soHdr.m_vectStrTbl[i].compare (p_soBody->m_strName)) {
			bItFound = true;
			break;
		}
		ullInd += p_soDoc.m_soHdr.m_vectStrTbl[i].length () + 1;
	}
	/* преобразуем индекс в последовательность */
	LongLong_toub_u_int32 (ullInd, p_strInd);
	/* если необходимо добавим строку в список */
	if (!bItFound)
		p_soDoc.m_soHdr.m_vectStrTbl.push_back (p_soBody->m_strName);
}
