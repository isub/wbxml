#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <map>

#include "decoder.h"
#include "encoder.h"
#include "wbxml.h"
#ifdef WIN32
#	define open _open
#	define O_RDONLY _O_RDONLY
#endif

#include "wbxml.h"
#include "utils/filereader/filereader.h"

int main (int argc, char *argv[])
{
	int iRetVal = 0;
	int iFnRes;
	std::string strInFile;
	std::string strAction;
	SFileInfo soFileInfo;
	CFileReader coFileReader;

	/* parse parameters */
	for (int i = 0; i < argc; i++) {
		if (0 == strncmp (argv[i], "infile=", 7)) {
			strInFile = &argv[i][7];
		} else if (0 == strncmp (argv[i], "action=", 7)) {
			strAction = &argv[i][7];
		}
	}

	/* chek parameters */
	if (0 == strAction.length())
		return iRetVal;
	if (0 == strAction.length())
		return iRetVal;

	/* read file */
	soFileInfo.m_strTitle = strInFile;
	iFnRes = coFileReader.OpenDataFile ("file", NULL, soFileInfo, NULL, NULL, NULL);
	if (iFnRes) {
		iRetVal = iFnRes;
		goto cleanup_and_exit;
	}

	if (strAction == "decode") {
		InitDecoderDict ();
		iRetVal = Decode (coFileReader);
	} else if (strAction == "encode") {
		InitEncoderDict ();
		iRetVal = Encode (coFileReader);
	}

cleanup_and_exit:
	coFileReader.CloseDataFile ();

	return iRetVal;
}
