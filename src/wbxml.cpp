#include "wbxml.h"

unsigned long long ub_u_int32_toLongLong (mb_u_int32 &p_mbValue, CFileReader &p_coFileReader, int &p_iResult)
{
	unsigned long long ullRetVal = 0;
	int iDataLen;
	int iFnRes;

	iDataLen = sizeof (p_mbValue);
	while (p_mbValue.m_bIndicator) {
		ullRetVal <<= 7;
		ullRetVal += p_mbValue.m_b06;
		iFnRes = p_coFileReader.ReadData ((unsigned char*)&p_mbValue, iDataLen);
		/* проверяем результат выполнения операции */
		if (iFnRes) {
			p_iResult = iFnRes;
			break;
		}
		/* проверяем достаточно ли нам полученных данных */
		if (iDataLen != sizeof (p_mbValue)) {
			p_iResult = ENODATA;
			break;
		}
	}
	if (0 == p_iResult) {
		/* добавляем последние биты */
		ullRetVal <<= 7;
		ullRetVal += p_mbValue.m_b06;
	}

	return ullRetVal;
}

void LongLong_toub_u_int32 (unsigned long long p_ullIn, std::basic_string<mb_u_int32> &p_strOut)
{
	mb_u_int32 mbLast;
	while (p_ullIn > 127) {
		mbLast.m_bIndicator = 1;
		mbLast.m_b06 = 127;
		p_strOut.append (&mbLast, 1);
		p_ullIn -= 127;
	}
	mbLast.m_uiValue = p_ullIn;
	p_strOut.append (&mbLast, 1);
}
