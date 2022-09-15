#include <iostream>

namespace CheekyLayer::rules
{
	class numbered_streambuf : public std::streambuf
	{
		private:
			std::streambuf* mySource;
			std::istream* myOwner;

			bool myIsAtStartOfLine = true;
			int myLineNumber = 0;
			int myColumnNumber = 0;

			char myBuffer;
		public:
			numbered_streambuf(std::streambuf* source) : mySource(source), myOwner(nullptr) {}
			numbered_streambuf(std::istream& owner) : mySource(owner.rdbuf()), myOwner(&owner)
			{
				myOwner->rdbuf(this);
			}
			~numbered_streambuf()
			{
				if(myOwner)
				{
					myOwner->rdbuf(mySource);
				}
			}

			int line() const
			{
				return myLineNumber;
			}

			int col() const
			{
				return myColumnNumber;
			}
		protected:
			int underflow() override
			{
				int ch = mySource->sbumpc();
				if(ch != EOF)
				{
					myBuffer = ch;
					setg(&myBuffer, &myBuffer, &myBuffer+1);
					if(myIsAtStartOfLine)
					{
						myLineNumber++;
						myColumnNumber = 0;
						if(ch == '#')
						{
							while((ch = mySource->sbumpc()) != '\n') { if(ch == EOF) return ch; }
							myIsAtStartOfLine = true;
							return underflow();
						}
					}
					else
					{
						myColumnNumber++;
					}
					myIsAtStartOfLine = myBuffer == '\n';
				}
				return ch;
			}
	};
}