/*

  Module: asf_redirection.cxx

  Provides redirection services where this is necessary.

*/

#include "../asfr.h"
#include "../helper.h"

enum
{
	mode_determinefiletype,
	mode_searchforbracket,
	mode_expectkeyword,
	mode_readkeyword,
	mode_expectoption,
	mode_readoption,
	mode_expectargumentoroption,
	mode_argumentstart,
	mode_argument,
	mode_argumentquotes,
	mode_expectclosingbrackets,
};

enum
{
	submode_none,
	submode_eatwhite
};

typedef enum
{
	unknownfile = 0,
	xmlfile,
	inifile,
	plainfile,
}
ASXFileType;

/* parse a redirection file (either from disk or from memory), builds a list of stream references (URLs) */
//int parse_redirection( unsigned char *filename, unsigned char *buffer, unsigned int redir_size, unsigned int maxtime, unsigned short portnum )
int parse_redirection( struct JOB_PARM *My_Parm, unsigned char *buffer, unsigned int redir_size )
{
	int result = 0;

	ASXFileType filetype;
	int error = 0;

	unsigned char *pos;
	int c;
	int iswhite;
	int isalpha;

	int mode;
	int submode;

	char *urlptr;
	char URLBuffer[4096];
	int my_argc;
	int urlcounter;
	//unsigned char *(my_argv[MAX_URLS]);

	char Keyword[128];
	char *keywordptr = NULL;

	char OptionBuffer[512];
	char *optionptr = NULL;
	int optionc = 0;
	char *(optionnamev[256]);
	char *(optionargv[256]);

	int keywordfinished;
	int termination;

	char TextLine[512];
	char *lineptr;
	char CurrentScope[128];

	my_argc = 0;
	urlcounter = 0;
	urlptr = URLBuffer;

	/* read file, if filename given and fill buffer */
	if ( strcmp(My_Parm->URL, "") && buffer == NULL)
	{
		/* if filename starts with http:// or mms://, it is not a filename */
		/* in this case, parse_redirection() returns with result 0 */
		if (    strnicmp("http://",  My_Parm->URL, 7) &&
			    strnicmp("mms://",   My_Parm->URL, 6) &&
			    strnicmp("mmsu://",  My_Parm->URL, 7) &&
			    strnicmp("mmst://",  My_Parm->URL, 7)    )
		{
			FILE *infile;
			infile = fopen( My_Parm->URL, "rb" );
			if (infile != NULL)
			{
				fseek(infile, 0, SEEK_END);
				redir_size = ftell(infile);

				fseek(infile, 0, SEEK_SET);
				if ((buffer = (unsigned char*)malloc(redir_size)) != NULL)
					fread(buffer, 1, redir_size, infile);

				myfclose( & infile);
			}
		}
		else
			return 0;
	}

	if (buffer != NULL && redir_size > 0)
	{
		pos = buffer;
		filetype = unknownfile;
		mode = mode_determinefiletype;
		submode = submode_eatwhite;
		keywordptr = Keyword;

		gui_showstatus(STATUS_INFORMATION, "Parsing redirectior...(%d bytes)\n", redir_size);
		/* explicity specify recording time, if different from 0
		if (maxtime != 0)
		{
			my_argv[my_argc++] = urlptr;
			urlptr += (sprintf(urlptr, "-m")+1);
			my_argv[my_argc++] = urlptr;
			urlptr += (sprintf(urlptr, "%d", maxtime)+1);
		}*/

		/* The following code is an XML parser with a very forgiving syntax check   */
		/* It doesn't comply to any "official" XML syntax rules, but it works fine. */

		/* It extracts keywords, options and arguments and can determine opening    */
		/* and closing of the scope of a keyword (<ASX>, </ASX>, <ASX OPTION />)    */
		/* example of syntax: <KEYWORD OPTION1=ARGUMENT OPTION2="ARGUMENT">         */

		/* character loop */
		while ( (!error) && (pos <= (buffer+redir_size)) && (filetype == unknownfile || filetype == xmlfile) )
		{
			if (pos == buffer+redir_size) {
				c = EOF;
				pos++;
			}
			else
				c = *pos++;

			if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == EOF )
				iswhite = 1;
			else
			    iswhite = 0;

			if ((c >= 'a' && c <= 'z') || c >= 'A' && c <= 'Z')
				isalpha = 1;
			else
			    isalpha = 0;

			if (submode == submode_eatwhite && iswhite) continue;

			keywordfinished = 0;

			switch(mode)
			{
			case mode_determinefiletype:
				if (c == '<') {
					/* fall through */
				}
				else {
					if (c == '[') {
						/* file is probably an INI file */
						filetype = inifile;
					}
					else {
						if (isalpha) {
							/* collect first plaintext keyword */
							*keywordptr++ = c;
							submode = submode_none;
						}
						else {
							*keywordptr++ = '\0';
							/* check if keyword matches "ASF" or any know protocol extension */
							if (!stricmp(Keyword, "ASF")   ||
								    stricmp(Keyword, "http")  ||
								    stricmp(Keyword, "mms")   ||
								    stricmp(Keyword, "mmst")  ||
								    stricmp(Keyword, "mmsu") ) {
								/* file type is probably a plain ASX file */
								filetype = plainfile;
							}
							else {
								gui_showstatus(STATUS_ERROR, "ASF syntax error 1!\n");
								error = 1;
							}
						}
					}
					break;
				}
				/* fall through */

			case mode_searchforbracket:
				if (c == '<') {
					termination=0;
					keywordptr = Keyword;
					optionptr = OptionBuffer;
					optionc = 0;
					mode = mode_expectkeyword;
					submode = submode_eatwhite;
				}
				break;

			case mode_expectkeyword:
				{
					if (c == '/') {
						termination = 1;
						break;
					}
					else {
						if (isalpha) {
							mode = mode_readkeyword;
							submode = submode_none;
							/* fall through */
						}
						else {
							gui_showstatus(STATUS_ERROR, "ASF syntax error 2!\n");
							error = 1;
							break;
						}
					}
				}
				/* fall through */

			case mode_readkeyword:
				if (!iswhite) {
					if (isalpha) {
						*keywordptr++ = c;
					}
					else {
						if (c == '/') {
							*keywordptr++ = '\0';
							termination = 1;
							mode = mode_expectclosingbrackets;
							submode = submode_eatwhite;
						}
						else {
							if (c == '>') {
								*keywordptr++ = '\0';
								keywordfinished = 1;
							}
							else {
								gui_showstatus(STATUS_ERROR, "ASF syntax error 3!\n");
								error = 1;
							}
						}
					}
				}
				else {
					*keywordptr++ = '\0';
					mode = mode_expectoption;
					submode = submode_eatwhite;
				}
				break;

			case mode_expectoption:
				{
					if (isalpha)
					{
						optionnamev[optionc] = optionptr;
						optionargv[optionc] = NULL;
						optionc++;
						mode = mode_readoption;
						submode = submode_none;
						/* fall through */
					}
					else {
						if (c == '/') {
							termination = 1;
							mode = mode_expectclosingbrackets;
							submode = submode_eatwhite;
							break;
						}
						else {
							if (c == '>') {
								keywordfinished = 1;
								break;
							}
							else {
								gui_showstatus(STATUS_ERROR, "ASF syntax error 4!\n");
								error = 1;
								break;
							}
						}
					}
				}
				/* fall through */

			case mode_readoption:
				{
					if (isalpha) {
						*optionptr++ = c;
						break;
					}
					else {
						*optionptr++ = '\0';
						mode = mode_expectargumentoroption;
						submode = submode_eatwhite;

						if (iswhite) break;

						/* fall through */
					}
				}
				/* fall through */

			case mode_expectargumentoroption:
				{
					if (c == '/') {
						termination = 1;
						mode = mode_expectclosingbrackets;
						submode = submode_eatwhite;
					}
					else {
						if (c == '>') {
							keywordfinished = 1;
						}
						else {
							if (c == '=') {
								optionargv[optionc-1] = optionptr;
								mode = mode_argumentstart;
								submode = submode_eatwhite;
							}
							else {
								gui_showstatus(STATUS_ERROR, "ASF syntax error 5!\n");
								error = 1;
							}
						}
					}
				}
				break;

			case mode_argumentstart:
				{
					if (c == '"') {
						mode = mode_argumentquotes;
						submode = submode_none;
						break;
					}
					else {
						if (c == '/') {
							gui_showstatus(STATUS_ERROR, "ASF syntax error 6!\n");
							error = 1;
							break;
						}
						else {
							mode = mode_argument;
							submode = submode_eatwhite;
							if (iswhite) break;
							/* fall through */
						}
					}
				}
				/* fall through */

			case mode_argument:
				{
					if (c == '"' || c == '=') {
						gui_showstatus(STATUS_ERROR, "ASF syntax error 7!\n");
						error = 1;
					}
					else {
						if (iswhite)
						{
							*optionptr++ = '\0';
							mode = mode_expectoption;
							submode = submode_eatwhite;
						}
						else {
							if (c == '/') {
								*optionptr++ = '\0';
								termination = 1;
								mode = mode_expectclosingbrackets;
								submode = submode_eatwhite;
							}
							else {
								if (c == '>') {
									*optionptr++ = '\0';
									keywordfinished = 1;
								}
								else {
									*optionptr++ = c;
									submode = submode_none;
								}
							}
						}
					}
				}
				break;

			case mode_argumentquotes:
				{
					if (c != '"') {
						*optionptr++ = c;
					}
					else {
						*optionptr++ = '\0';

						mode = mode_expectoption;
						submode = submode_eatwhite;
					}
				}
				break;

			case mode_expectclosingbrackets:
				{
					if (c == '>') {
						keywordfinished = 1;
					}
					else {
						gui_showstatus(STATUS_INFORMATION, "ASF syntax error 8!\n");
						error = 1;
					}
				}
				break;
			}

			if (keywordfinished)
			{
				/* begin to search for next keyword starting with a "<" */
				mode = mode_searchforbracket;
				submode = submode_eatwhite;

#if 0 																																																/* just for debugging */
				int i;

				if (termination)
					gui_logtext("Keyword: /%s\n", Keyword);
				else
				    gui_logtext("Keyword %s\n", Keyword);
				for (i=0; i<optionc; i++)
				{
					if (optionargv[i] == NULL)
						gui_logtext("option #%d %s\n", i+1, optionnamev[i]);
					else
					    gui_logtext("option #%d %s = %s\n", i+1, optionnamev[i], optionargv[i]);
				}
#endif

				if (!stricmp(Keyword, "ASX"))
				{
					/* signal the calling main_function that we parsed this ASX file */
					result = 1;
				}

				if (!stricmp(Keyword, "Entry") && (!termination))
				{
					/* set a marker for every new entry */

					/* this is because one entry can have multiple references */
					/* pointing to the same stream on different servers       */
					/* (or lower-bitrate streams) for redundancy              */

					/* this marker allows us to download only one stream of   */
					/* these multiple references                              */

					/*my_argv[my_argc++] = urlptr;
					urlptr += (sprintf(urlptr, "-e")+1);*/
				}

				if ((!stricmp(Keyword, "Ref")) || (!stricmp(Keyword, "EntryRef")))
				{
					if (optionc > 0)
					{
						if ( (!stricmp("href", optionnamev[0])) && (optionargv[0] != NULL) )
						{
							/* store this URL for later download */
							/*my_argv[my_argc++] = urlptr;
							urlptr += (sprintf(urlptr, "%s", optionargv[0])+1);
							urlcounter++;*/
							strcpy(My_Parm->URL, optionargv[0]);
						}
					}
				}
			}

			if (error)
			{
				/* try to recover after an error */
				mode = mode_searchforbracket;
				submode = submode_eatwhite;
				error = 0;
			}
		}

		if (filetype == inifile)
		{
			/* signal the calling main_function that we parsed this INI file */
			result = 1;

			pos = buffer;
			lineptr = TextLine;
			strcpy(CurrentScope, "");

			/* character loop */
			while ( (!error) && ( pos <= (buffer+redir_size) ) )
			{
				int linecomplete = 0;

				if (pos == buffer+redir_size) {
					c = EOF;
					pos++;
				}
				else
					c = *pos++;


				if ((c != '\r') && (c != '\n') && (c != EOF))
					*lineptr++ = c;
				else
				    *lineptr++ = 0;

				if (c == '\n' || c == EOF)
				{
					lineptr = TextLine;
					linecomplete = 1;
				}

				if (linecomplete)
				{
					if (TextLine[0] == '[')
					{
						strcpy(CurrentScope, TextLine);

						if (!stricmp(CurrentScope, "[Reference]"))
						{
							/* set new entry marker (-e option) */
							/*my_argv[my_argc++] = urlptr;
							urlptr += (sprintf(urlptr, "-e")+1);*/
							/* all following URLs belong to this single entry */
						}
					}
					else
					    {
						if (!stricmp(CurrentScope, "[Reference]"))
						{
							/* URLs begin with with REFxx= where xx is any number */
							if (!strnicmp(TextLine, "REF", 3))
							{
								char *ptr = TextLine+3;
								/* skip numbers */
								while ((*ptr) >= '0' && (*ptr) <= '9') ptr++;
								if ((*ptr) == '=')
								{
									ptr++;

									/* save the resulting URL string */
									/*my_argv[my_argc++] = urlptr;
									urlptr += (sprintf(urlptr, "%s",ptr)+1);
									urlcounter++;*/
									strcpy(My_Parm->URL, ptr);
								}
							}
						}
					}
				}
			}
		}

		if (filetype == plainfile)
		{
			/* signal the calling main_function that we parsed this INI file */
			result = 1;

			pos = buffer;
			lineptr = TextLine;

			/* set new entry marker (-e option) */
			/*my_argv[my_argc++] = urlptr;
			urlptr += (sprintf(urlptr, "-e")+1);*/
			/* all following URLs belong to this single entry */

			/* character loop */
			while ( (!error) && ( pos <= (buffer+redir_size) ) )
			{
				int linecomplete = 0;

				if (pos == buffer+redir_size) {
					c = EOF;
					pos++;
				}
				else
					c = *pos++;

				if ((c != '\r') && (c != '\n') && (c != EOF))
					*lineptr++ = c;
				else
				    *lineptr++ = 0;

				if (c == '\n' || c == EOF)
				{
					lineptr = TextLine;
					linecomplete = 1;
				}

				if (linecomplete)
				{
					char *ptr = TextLine;

					/* eat whitespaces */
					while( (*ptr == ' ') || (*ptr == '\t') ) ptr++;

					/* don't know if multiple lines are allowed in this */
					/* plain ASX file format but who cares */
					if ((!strnicmp(ptr, "ASF ",  4)) ||
						    (!strnicmp(ptr, "ASF\t", 4))   )
					{
						ptr += 4;

						/* eat whitespaces */
						while( (*ptr == ' ') || (*ptr == '\t') ) ptr++;

						/* save the resulting URL string */
						/*my_argv[my_argc++] = urlptr;
						urlptr += (sprintf(urlptr, "%s",ptr)+1);
						urlcounter++;*/
						strcpy(My_Parm->URL, ptr);
					}
					else
					    {
						/* some providers use ASX files that contain the URL only */
						if ((!strnicmp(ptr, "http://",  7)) ||
							    (!strnicmp(ptr, "mms://",   6)) ||
							    (!strnicmp(ptr, "mmsu://",  7)) ||
							    (!strnicmp(ptr, "mmst://",  7)) )
						{
							/* save the resulting URL string */
							/*my_argv[my_argc++] = urlptr;
							urlptr += (sprintf(urlptr, "%s",ptr)+1);
							urlcounter++;*/
							strcpy(My_Parm->URL, ptr);
						}
					}
				}
			}
		}

#if 0 																								/* just for debugging */
		if (my_argc > 1)
		{
			int url;

			gui_logtext("List of arguments for main_function()\n");

			for (url = 0; url < my_argc; url++)
			{
				gui_criticalerror("arg #%d: %s\n", url, my_argv[url]);
			}
		}
#endif
		/* recursively call main_function() with the extracted URLs */
		if (urlcounter > 0)
			;//main_function(my_argc, (char**)my_argv);
	}

	return result;
}
