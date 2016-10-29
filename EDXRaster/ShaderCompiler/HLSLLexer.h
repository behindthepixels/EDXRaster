#pragma once

#include "EDXPrerequisites.h"
#include "CompilerCommon.h"

namespace EDX
{
	namespace ShaderCompiler
	{
		class HLSLLexer
		{
		private:
			string mFileName;
			string mString;
			const char* mpCurrent;
			const char* mpEnd;
			const char* mpCurrentLineStart;
			int mLine;

		public:
			void Init(const char* fileName)
			{
				mFileName = fileName;
				// Init mString and other pointers here
			}

			Array<HLSLToken> Tokenize(const char* fileName,
				const string& str,
				Array<CompileError>& ErrorList)
			{
				Array<HLSLToken> ret;

				while (HasCharsAvailable())
				{
					SkipWhitespaceAndEmptyLines();


				}
			}

		private:
			HLSLToken NextToken()
			{
				
			}

			// Utils
			__forceinline bool IsSpaceOrTab(char ch)
			{
				return ch == ' ' || ch == '\t';
			}

			__forceinline bool IsEOL(char ch)
			{
				return ch == '\r' || ch == '\n';
			}

			__forceinline bool IsSpaceOrTabOrEOL(char ch)
			{
				return IsEOL(ch) || IsSpaceOrTab(ch);
			}

			__forceinline bool IsChar(char ch)
			{
				return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
			}

			__forceinline bool IsDigit(char ch)
			{
				return ch >= '0' && ch <= '9';
			}

			__forceinline bool IsHexDigit(char ch)
			{
				return IsDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
			}

			__forceinline bool IsCharOrDigit(char ch)
			{
				return IsChar(ch) || IsDigit(ch);
			}

			bool HasCharsAvailable() const
			{
				return mpCurrent < mpEnd;
			}

			char Peek() const
			{
				if (HasCharsAvailable())
				{
					return *mpCurrent;
				}

				return 0;
			}

			char Peek(int32 Delta) const
			{
				assert(Delta > 0);
				if (mpCurrent + Delta < mpEnd)
				{
					return mpCurrent[Delta];
				}

				return 0;
			}

			void SkipWhitespaceInLine()
			{
				while (HasCharsAvailable())
				{
					auto Char = Peek();
					if (!IsSpaceOrTab(Char))
					{
						break;
					}

					++mpCurrent;
				}
			}

			void SkipToNextLine()
			{
				while (HasCharsAvailable())
				{
					auto Char = Peek();
					++mpCurrent;
					if (Char == '\r' && Peek() == '\n')
					{
						++mpCurrent;
						break;
					}
					else if (Char == '\n')
					{

						break;
					}
				}

				++mLine;
				mpCurrentLineStart = mpCurrent;
			}

			void SkipWhitespaceAndEmptyLines()
			{
				while (HasCharsAvailable())
				{
					SkipWhitespaceInLine();
					auto Char = Peek();
					if (IsEOL(Char))
					{
						SkipToNextLine();
					}
					else
					{
						auto NextChar = Peek(1);
						if (Char == '/' && NextChar == '/')
						{
							// C++ comment
							mpCurrent += 2;
							this->SkipToNextLine();
							continue;
						}
						else if (Char == '/' && NextChar == '*')
						{
							// C Style comment, eat everything up to * /
							mpCurrent += 2;
							bool bClosedComment = false;
							while (HasCharsAvailable())
							{
								if (Peek() == '*')
								{
									if (Peek(1) == '/')
									{
										bClosedComment = true;
										mpCurrent += 2;
										break;
									}

								}
								else if (Peek() == '\n')
								{
									SkipToNextLine();

									// Don't increment current!
									continue;
								}

								++mpCurrent;
							}
							//@todo-rco: Error if no closing * / found and we got to EOL
							//check(bClosedComment);
						}
						else
						{
							break;
						}
					}
				}
			}
		};
	}
}