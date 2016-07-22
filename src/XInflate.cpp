#include "stdafx.h"
#include "XInflate.h"

/**********************************************************************************

                                    -= history =-

* 2010/4/5 �ڵ� ����
* 2010/4/9 1�� �ϼ�
	- �ӵ� �� (34.5MB ¥�� gzip ����, Q9400 CPU)
		. zlib : 640ms
		. xinflate : 1900ms

* 2010/4/29 
	- �ӵ� ��� ����ȭ 
		. �Ϻ� �Լ� ȣ���� ��ũ��ȭ ��Ű�� : 1900ms
		. ��Ʈ ��Ʈ���� ���� ����ȭ ��Ű��: 1800ms
		. Write �Լ� ��ũ��ȭ: 1750ms
		. Window ��� ����ȭ: 1680ms
		. Window ��ũ��ȭ + ���� ����ȭ : 1620ms
		. InputBuffer ���� ����ȭ : 1610ms
		. ��� ������ ���� ����ȭ : 1500ms;
		. while() �������� m_bCompleted �� ���� : 1470ms;
		. while() ���������� m_error �� ���� : 1450ms
		. m_state ���� ����ȭ : 1450ms
		. m_windowLen, m_windowDistCode ���� ����ȭ : 1390ms

	- ���� ����ȭ �� �ӵ� ���� �� (34.5MB ¥�� gzip ����, Q9400 CPU)
		. zlib : 640MS
		. Halibut : 1750ms
		. XInflate : 1370ms

* 2010/4/30
	- �ڵ� ����
	- v1.0 ���� ����

* 2010/5/11
	- static table �� �ʿ��Ҷ��� �����ϵ��� ����
	- ����ȭ
		. ����ȭ �� : 1340ms
		. CHECK_TABLE ���� : 1330ms
		. FILLBUFFER() ����: 1320ms
		. table�� ��ũ�� ����Ʈ���� �迭�� ���� : 1250ms

* 2010/5/12
	- ����ȭ ���
		. STATE_GET_LEN �� break ���� : 1220ms
		. STATE_GET_DISTCODE �� break ���� : 1200ms
		. STATE_GET_DIST �� break ���� : 1170ms

* 2010/5/13
	- ����ȭ ���
		. FastInflate() �� �и��� FILLBUFFER_FAST, HUFFLOOKUP_FAST ���� : 1200ms
		. ������ Ʈ�� ó�� ��� ����(���� ���̺� ������ Ʈ�� Ž���� ���ְ� �޸� ��뷮�� ���̰�) : 900ms
		. HUFFLOOKUP_FAST �ٽ� ���� : 890ms
		. WRITE_FAST ���� : 810ms
		. lz77 ������ ��½� while() �� do-while �� ���� : 800ms

* 2010/5/19
	- ��� ���۸� ���� alloc ��� �ܺ� ���۸� �̿��� �� �ֵ��� ��� �߰�
	- m_error ���� ����

* 2010/5/25
	- �ܺι��� ��� ����� ���� ����
	- direct copy �� �ణ ����

* 2010/08/31
	- ���̺� ������ �̻��Ѱ� ������ ���������ϵ��� ����

* 2011/08/01
	- STATE_GET_LEN ������ FILLBUFFER �� ���ϰ� STATE_GET_DISTCODE �� �Ѿ�鼭 ������ ����� Ǯ�� ���ϴ� ��찡 �ִ�
	  ���� ���� ( Inflate() �� FastInflate() �ΰ� �� )

* 2011/08/16
	- coderecord �� ����ü�� ������� �ʰ�, len �� dist�� �����ϵ��� ����
		142MB ���� : 1820ms -> 1720ms �� ������
    - HUFFLOOKUP_FAST ���� m_pCurrentTable �� �������� �ʰ�, pItems �� �����ϵ��� ����
		1720ms -> 1700ms �� ������ (zlib �� 1220ms)

* 2012/01/28
	- windowDistCode �� 32, 33 ���� �ɶ� ���� ó�� �ڵ� ����
	- _CreateTable ���� ���̺��� �ջ�� ��� ���� ó�� �ڵ� ���

* 2012/02/7
	- XFastHuffItem::XFastHuffItem() �� �ּ����� ���Ҵ��� Ǯ����.

* 2016/7/21
	- ������ zlib ��Ÿ���� ���� ����� �ݹ� ȣ�� ������� ����
	- ���� ���۵� ���� �Ҵ����� �ʰ�, ��� ���۸� ���� ����ϵ��� ����

* 2016/7/22
	- BYPASS �Ҷ� ���� ��� WRITE_BLOCK �� memcpy �� �ӵ� ���

***********************************************************************************/


// DEFLATE �� ���� Ÿ��
enum BTYPE
{
	BTYPE_NOCOMP			= 0,
	BTYPE_FIXEDHUFFMAN		= 1,
	BTYPE_DYNAMICHUFFMAN	= 2,
	BTYPE_RESERVED			= 3		// ERROR
};


#define LENCODES_SIZE		29
#define DISTCODES_SIZE		30

static int lencodes_extrabits	[LENCODES_SIZE]  = {  0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   0};
static int lencodes_min			[LENCODES_SIZE]  = {  3,   4,   5,   6,   7,   8,   9,  10,  11,  13,  15,  17,  19,  23,  27,  31,  35,  43,  51,  59,  67,  83,  99, 115, 131, 163, 195, 227, 258};
static int distcodes_extrabits	[DISTCODES_SIZE] = {0, 0, 0, 0, 1, 1,  2,  2,  3,  3,  4,  4,  5,   5,   6,   6,   7,   7,   8,    8,    9,    9,   10,   10,   11,   11,    12,    12,    13,    13};
static int distcodes_min		[DISTCODES_SIZE] = {1, 2, 3, 4, 5, 7,  9, 13, 17, 25, 33, 49, 65,  97, 129, 193, 257, 385, 513,  769, 1025, 1537, 2049, 3073, 4097, 6145,  8193, 12289, 16385, 24577};


// code length 
static const unsigned char lenlenmap[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};


// ��ƿ ��ũ��
#define HUFFTABLE_VALUE_NOT_EXIST	-1
#ifndef ASSERT
#	define ASSERT(x) {}
#endif
#define SAFE_DEL(x) if(x) {delete x; x=NULL;}
#define SAFE_FREE(x) if(x) {free(x); x=NULL;}



////////////////////////////////////////////////
//
// ��Ʈ ��Ʈ�� ��ũ��ȭ
//
#define BS_EATBIT()								\
	bits & 0x01;								\
	bitLen --;									\
	bits >>= 1;									\
	if(bitLen<0) ASSERT(0);

#define BS_EATBITS(count)						\
	(bits & ((1<<(count))-1));					\
	bitLen -= count;							\
	bits >>= count;								\
	if(bitLen<0) ASSERT(0);

#define BS_REMOVEBITS(count)					\
	bits >>= (count);							\
	bitLen -= (count);							\
	if(bitLen<0) ASSERT(0); 

#define BS_MOVETONEXTBYTE						\
	BS_REMOVEBITS((bitLen % 8));

#define BS_ADDBYTE(byte)						\
	bits |= (byte) << bitLen;					\
	bitLen += 8;
//
// ��Ʈ ��Ʈ�� ��ũ�� ó��
//
////////////////////////////////////////////////


////////////////////////////////////////////////
//
// ������ ��ũ��
//
#ifdef _DEBUG
#	define ADD_INPUT_COUNT			m_inputCount ++
#	define ADD_OUTPUT_COUNT			m_outputCount ++
#	define ADD_OUTPUT_COUNTX(x)		m_outputCount += x
#else
#	define ADD_INPUT_COUNT	
#	define ADD_OUTPUT_COUNT
#	define ADD_OUTPUT_COUNTX(x)
#endif


////////////////////////////////////////////////
//
// �ݺ� �۾� ��ũ��ȭ
//
#define FILLBUFFER()							\
	while(bitLen<=24)							\
	{											\
		if(inBufferRemain==0)					\
			break;								\
		BS_ADDBYTE(*inBuffer);					\
		inBufferRemain--;						\
		inBuffer++;								\
		ADD_INPUT_COUNT;						\
	}											\
	if(bitLen<=0)								\
		goto END;


#define HUFFLOOKUP(result, pTable)								\
		/* ����Ÿ ���� */										\
		if(pTable->bitLenMin > bitLen)							\
		{														\
			result = -1;										\
		}														\
		else													\
		{														\
			pItem = &(pTable->pItem[pTable->mask & bits]);		\
			/* ����Ÿ ���� */									\
			if(pItem->bitLen > bitLen)							\
			{													\
				result = -1;									\
			}													\
			else												\
			{													\
				result = pItem->symbol;							\
				BS_REMOVEBITS(pItem->bitLen);					\
			}													\
		}


// ��� ���ۿ� �ѹ���Ʈ ����
#define WRITE(byte)												\
	ADD_OUTPUT_COUNT;											\
	WIN_ADD;													\
	CHECK_AND_FLUSH_OUT_BUFFER									\
	*outBufferCur = byte;										\
	outBufferCur++;


// ��� ���۰� ��á���� -> ���� + ��� ���� ��ġ �̵� �ʱ�ȭ + ���� ����Ÿ�� ��� ���� ������ ����
#define CHECK_AND_FLUSH_OUT_BUFFER								\
	if(outBufferCur>=outBufferEnd)								\
	{															\
		if(stream->Write(windowStartPos, (int)(outBufferCur - windowStartPos))==FALSE)		\
			return XINFLATE_ERR_USER_STOP;						\
		int _dist = (int)(outBufferCur - windowCurPos);			\
		memcpy(windowStartPos-DEFLATE_WINDOW_SIZE, outBufferCur - DEFLATE_WINDOW_SIZE, DEFLATE_WINDOW_SIZE);	\
		windowCurPos = windowStartPos-_dist ;					\
		outBufferCur = windowStartPos;							\
	}															\
	


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                              FastInflate
//


// �Է� ���۰� ������� üũ���� �ʴ´�.
#define FILLBUFFER_FAST()								\
	while(bitLen<=24)									\
	{													\
		BS_ADDBYTE(*inBuffer);							\
		inBufferRemain--;								\
		inBuffer++;										\
		ADD_INPUT_COUNT;								\
	}													\

// ��Ʈ��Ʈ�� ����Ÿ(bits + bitLen)�� ������� üũ���� �ʴ´�.
#define HUFFLOOKUP_FAST(result)							\
		pItem = &(pItems[mask & bits]);					\
		result = pItem->symbol;							\
		BS_REMOVEBITS(pItem->bitLen);					


// ��� ���۰� ������� üũ���� �ʴ´�.
#define WRITE_FAST(byte)							\
	ADD_OUTPUT_COUNT;								\
	WIN_ADD;										\
	*outBufferCur = byte;							\
	outBufferCur++;												


#define WRITE_BLOCK(in, len)						\
	ADD_OUTPUT_COUNTX(len);							\
	memcpy(outBufferCur, in, len);					\
	windowCurPos +=len;								\
	outBufferCur +=len;

////////////////////////////////////////////////
//
// ������ ��ũ��ȭ
//

#define WIN_ADD				(windowCurPos++)
#define WIN_GETBUF(dist)	(windowCurPos - dist)

//
// ������ ��ũ��ȭ
//
////////////////////////////////////////////////



/////////////////////////////////////////////////////
//
// ������ Ʈ���� ���� Ž���� �� �ְ� �ϱ� ���ؼ�
// Ʈ�� ��ü�� �ϳ��� �迭�� ����� ������.
//   

struct XFastHuffItem									// XFastHuffTable �� ������
{
	// ���� ����Ÿ���� ���� �����÷ο츦 ���� ���ؼ� �׻� �ʱ�ȭ�� ����� �Ѵ� ��.��
	XFastHuffItem() 
	{
		bitLen = 0;
		symbol = HUFFTABLE_VALUE_NOT_EXIST;
	}
	int		bitLen;										// ��ȿ�� ��Ʈ��
	int		symbol;										// ��ȿ�� �ɺ�
};

class XFastHuffTable
{
public :
	XFastHuffTable()
	{
		pItem = NULL;
	}
	~XFastHuffTable()
	{
		if(pItem){ delete[] pItem; pItem=NULL;}
	}
	// �迭 ����
	void	Create(int _bitLenMin, int _bitLenMax)
	{
		if(pItem) ASSERT(0);							// �߻� �Ұ�

		bitLenMin = _bitLenMin;
		bitLenMax = _bitLenMax;
		mask	  = (1<<bitLenMax) - 1;					// ����ũ
		itemCount = 1<<bitLenMax;						// ���� ������ �ִ� ���̺� ũ��
		pItem     = new XFastHuffItem[itemCount];		// 2^maxBitLen ��ŭ �迭�� �����.
	}
	// �ɺ� ����Ÿ�� ������ ��ü �迭�� ä���.
	BOOL	SetValue(int symbol, int bitLen, UINT bitCode)
	{
		/*
			���� ������ Ʈ�� ��忡�� 0->1->1 �̶�� ����Ÿ�� 'A' ���
			symbol = 'A',   bitLen = 3,   bitCode = 3  �� �Ķ���ͷ� ���޵ȴ�.

			* ���� bitstream �� �������� ������ ������ ���߿� ������ ���� �ϱ� ���ؼ�
			  0->1->1 �� 1<-1<-0 ���� �����´�.

			* ���� bitLenMax �� 5 ��� �������� 1<-1<-0 �� �� �� 2bit �� ���� ������
			  00110, 01110, 10110, 11110 �� ���ؼ��� ������ �ɺ��� ������ �� �ֵ��� �����.
		*/
		//::Ark_DebugW(L"SetValue: %d %d %d", symbol, bitLen, bitCode);
		if(bitCode>=((UINT)1<<bitLenMax))
		{ASSERT(0); return FALSE;}			// bitLenmax �� 3 �̶��.. 111 ������ �����ϴ�


		UINT revBitCode = 0;
		// ������
		int i;
		for(i=0;i<bitLen;i++)
		{
			revBitCode <<= 1;
			revBitCode |= (bitCode & 0x01);
			bitCode >>= 1;
		}

		int		add2code = (1<<bitLen);		// bitLen �� 3 �̶�� add2code ���� 1000(bin) �� ����

		// �迭 ä���
		for(;;)
		{
#ifdef _DEBUG
			if(revBitCode>=itemCount) ASSERT(0);

			if(pItem[revBitCode].symbol!=  HUFFTABLE_VALUE_NOT_EXIST) 
			{ ASSERT(0); return FALSE;}
#endif
			pItem[revBitCode].bitLen = bitLen;
			pItem[revBitCode].symbol = symbol;

			// ���� ������ bit code �� ó���ϱ� ���ؼ� ���� ��� ���Ѵ�.
			revBitCode += add2code;

			// ���� ������ ������ ��� ��� ������
			if(revBitCode >= itemCount)
				break;
		}
		return TRUE;
	}

	XFastHuffItem*	pItem;							// (huff) code 2 symbol ������, code�� �迭�� ��ġ ������ �ȴ�.
	int				bitLenMin;						// ��ȿ�� �ּ� ��Ʈ��
	int				bitLenMax;						// ��ȿ�� �ִ� ��Ʈ��
	UINT			mask;							// bitLenMax �� ���� bit mask
	UINT			itemCount;						// bitLenMax �� ���� ������ �ִ� ������ ����
};

static const char* xinflate_copyright = 
"[XInflate - Copyright(C) 2016, by kippler]";

XInflate::XInflate()
{
	m_pStaticInfTable = NULL;
	m_pStaticDistTable = NULL;
	m_pDynamicInfTable = NULL;
	m_pDynamicDistTable = NULL;
	m_pCurrentTable = NULL;
	m_pCurrentDistTable = NULL;

	m_pLenLenTable = NULL;
	m_dicPlusOutBuffer = NULL;
	m_outBuffer = NULL;
	m_inBuffer = NULL;
	m_copyright = NULL;
}

XInflate::~XInflate()
{
	Free();
}

// ���� �޸� alloc ����
void XInflate::Free()
{
	SAFE_DEL(m_pStaticInfTable);
	SAFE_DEL(m_pStaticDistTable);
	SAFE_DEL(m_pDynamicInfTable);
	SAFE_DEL(m_pDynamicDistTable);
	SAFE_DEL(m_pLenLenTable);

	SAFE_FREE(m_dicPlusOutBuffer);
	SAFE_FREE(m_inBuffer);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         ���� ���� �ʱ�ȭ
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:17:46 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
XINFLATE_ERR XInflate::Init()
{
	SAFE_DEL(m_pLenLenTable);
	SAFE_DEL(m_pDynamicInfTable);
	SAFE_DEL(m_pDynamicDistTable);

	m_pCurrentTable = NULL;
	m_pCurrentDistTable = NULL;

	m_bFinalBlock = FALSE;
	m_copyright = xinflate_copyright;

	// ��� ���� ���
	if (m_dicPlusOutBuffer == NULL)
	{
		m_dicPlusOutBuffer = (BYTE*)malloc(DEFAULT_OUTBUF_SIZE + DEFLATE_WINDOW_SIZE);
		if (m_dicPlusOutBuffer == NULL){ASSERT(0); return XINFLATE_ERR_ALLOC_FAIL;}

		/*
		���� ���� alloc �� ��ġ���� ������ ũ�⸸ŭ ���ʿ� ���� �����Ѵ�.

		alloc ��ġ
		|		outBuffer ��ġ
		��       ��
		+-------+-----------------------------+
		| ����  |                             |
		+-------+-----------------------------+
		*/

		// ������ ������ ������ ���۽�����ġ
		m_outBuffer = m_dicPlusOutBuffer + DEFLATE_WINDOW_SIZE;
	}

	if (m_inBuffer == NULL)
	{
		m_inBuffer = (BYTE*)malloc(DEFAULT_INBUF_SIZE);
		if (m_inBuffer == NULL){ASSERT(0); return XINFLATE_ERR_ALLOC_FAIL;}
	}

#ifdef _DEBUG
	m_inputCount = 0;
	m_outputCount = 0;
#endif

	return XINFLATE_ERR_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         ���� ���� - ȣ���� Init() �� �ݵ�� ȣ���� ��� �Ѵ�.
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:18:55 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
#define	DOERR(x) { ASSERT(0); ret = x; goto END; }
XINFLATE_ERR XInflate::Inflate(IDecodeStream* stream)
{
	XINFLATE_ERR	ret = XINFLATE_ERR_OK;

	// �ʱ�ȭ
	ret = Init();
	if (ret != XINFLATE_ERR_OK) return ret;

	UINT		bits=0;
	int			bitLen=0;
	STATE		state= STATE_START;
	int			windowLen=0;
	int			windowDistCode=0;
	int			symbol=0;
	BYTE*		inBuffer = m_inBuffer;
	int			inBufferRemain = 0;
	UINT		uncompRemain = 0;

	BYTE*		outBufferCur = m_outBuffer;
	BYTE*		outBufferEnd = m_outBuffer + DEFAULT_OUTBUF_SIZE;
	BYTE*		windowStartPos = m_outBuffer;
	BYTE*		windowCurPos = m_outBuffer;

	// ���� ����
	int					extrabits;
	int					dist;
	XFastHuffItem*		pItem;			// HUFFLOOKUP() ���� ���
	BYTE				byte;			// �ӽ� ����
	XINFLATE_ERR		err;

	// ���� ���鼭 ���� ����
	for(;;)
	{
		// ���� ä���
		if (inBufferRemain == 0)
		{
			inBuffer = m_inBuffer;
			inBufferRemain = stream->Read(inBuffer, DEFAULT_INBUF_SIZE);
		}
		
		FILLBUFFER();

		switch(state)
		{
			// ��� �м� ����
		case STATE_START :
			if(bitLen<3) 
				goto END;

			// ������ �� ����
			m_bFinalBlock = BS_EATBIT();

			// ��Ÿ�� ����� 2bit 
			{
				int bType = BS_EATBITS(2);

				if(bType==BTYPE_DYNAMICHUFFMAN)
					state = STATE_DYNAMIC_HUFFMAN;
				else if(bType==BTYPE_NOCOMP)
					state = STATE_NO_COMPRESSION;
				else if(bType==BTYPE_FIXEDHUFFMAN)
					state = STATE_FIXED_HUFFMAN;
				else
					DOERR(XINFLATE_ERR_HEADER);
			}
			break;

			// ���� �ȵ�
		case STATE_NO_COMPRESSION :
			BS_MOVETONEXTBYTE;
			state = STATE_NO_COMPRESSION_LEN;
			break;

		case STATE_NO_COMPRESSION_LEN :
			// LEN
			if(bitLen<16) 
				goto END;
			uncompRemain = BS_EATBITS(16);
			state = STATE_NO_COMPRESSION_NLEN;

			break;

		case STATE_NO_COMPRESSION_NLEN :
			// NLEN
			if(bitLen<16) 
				goto END;
			{
				UINT32 nlen = BS_EATBITS(16);
				// one's complement 
				if( (nlen^0xffff) != uncompRemain) 
					DOERR(XINFLATE_ERR_INVALID_NOCOMP_LEN);
			}
			state = STATE_NO_COMPRESSION_BYPASS;
			break;

		case STATE_NO_COMPRESSION_BYPASS :
			// ����Ÿ ��������
			if(bitLen<8) 
				goto END;

			//////////////////////////////////////////////
			//
			// ���� �ڵ�
			//
			/*
			{
				byte = BS_EATBITS(8);
				WRITE(byte);
			}
			uncompRemain--;
			*/
			if(bitLen%8!=0) ASSERT(0);			// �߻��Ұ�, �׳� Ȯ�ο�

			//////////////////////////////////////////////
			//
			// �Ʒ��� ������ �ڵ�. �׷��� �� ���̰� ���� ��.��
			//

			// ��Ʈ ��Ʈ���� ���� ����
			while(bitLen && uncompRemain)
			{
				byte = BS_EATBITS(8);
				WRITE(byte);
				uncompRemain--;
			}

			// ������ ����Ÿ�� ����Ʈ �״�� ����
			{
				int	toCopy, toCopy2;
				toCopy = toCopy2 = min((int)uncompRemain, inBufferRemain);

				/* �����ڵ�
				while(toCopy)
				{
					WRITE(*inBuffer);
					inBuffer++;
					toCopy--;
				}
				*/

				if(outBufferEnd - outBufferCur > toCopy)
				{
					// ��� ���۰� ����� ��� - ��� ���۰� ������� ���θ� üũ���� �ʴ´�.
					/*
					while(toCopy)
					{
						WRITE_FAST(*inBuffer);
						inBuffer++;
						toCopy--;
					}
					*/
					WRITE_BLOCK(inBuffer, toCopy);
					inBuffer += toCopy;
					toCopy = 0;
				}
				else
				{
					while(toCopy)
					{
						WRITE(*inBuffer);
						inBuffer++;
						toCopy--;
					}
				}

				uncompRemain-=toCopy2;
				inBufferRemain-=toCopy2;
			}
			//
			// ������ �ڵ� ��
			//
			//////////////////////////////////////////////

			if(uncompRemain==0)
			{
				if(m_bFinalBlock)
					state = STATE_COMPLETED;
				else
					state = STATE_START;
			}
			break;

			// ���� ������
		case STATE_FIXED_HUFFMAN :
			if(m_pStaticInfTable==NULL)
				CreateStaticTable();

			m_pCurrentTable = m_pStaticInfTable;
			m_pCurrentDistTable = m_pStaticDistTable;
			state = STATE_GET_SYMBOL;
			break;

			// ���� ��������
		case STATE_GET_LEN :
			// zlib �� inflate_fast ȣ�� ���� �䳻����
			if(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)
			{
				if(symbol<=256)	ASSERT(0);

				XINFLATE_ERR result = FastInflate(stream, inBuffer, inBufferRemain, 
										outBufferCur, 
										outBufferEnd,
										windowStartPos, windowCurPos,
										bits, bitLen, state, windowLen, windowDistCode, symbol);

				if(result!=XINFLATE_ERR_OK)
					return result;

				break;
			}

			extrabits = lencodes_extrabits[symbol - 257];
			if (bitLen < extrabits) 
				goto END;

			// RFC 1951 3.2.5
			// �⺻ ���̿� extrabit ��ŭ�� ��Ʈ�� ������ ���ϸ� ��¥ ���̰� ���´�
			windowLen = lencodes_min[symbol - 257] + BS_EATBITS(extrabits);

			state = STATE_GET_DISTCODE;
			FILLBUFFER();
			//break;	�ʿ� ����..

			// �Ÿ� �ڵ� ��������
		case STATE_GET_DISTCODE :
			HUFFLOOKUP(windowDistCode, m_pCurrentDistTable);

			if(windowDistCode<0)
				goto END;

			//if(windowDistCode==30 || windowDistCode==31)	// 30 �� 31�� ���� �� ����. RFC1951 3.2.6
			if(windowDistCode>=30)							
				DOERR(XINFLATE_ERR_INVALID_DIST);

			state = STATE_GET_DIST;

			FILLBUFFER();
			//break;		// �ʿ����

			// �Ÿ� ��������
		case STATE_GET_DIST:
			extrabits = distcodes_extrabits[windowDistCode];

			// DIST ���ϱ�
			if(bitLen<extrabits)
				goto END;

			dist = distcodes_min[windowDistCode] + BS_EATBITS(extrabits);

			// lz77 ���
			while(windowLen)
			{
				byte = *WIN_GETBUF(dist);
				WRITE(byte);
				windowLen--;
			}
	
			state = STATE_GET_SYMBOL;

			FILLBUFFER();
			//break;		// �ʿ� ����

			// �ɺ� ��������.
		case STATE_GET_SYMBOL :
			HUFFLOOKUP(symbol, m_pCurrentTable);

			if(symbol<0) 
				goto END;
			else if(symbol<256)
			{
				byte = (BYTE)symbol;
				WRITE(byte);
				break;
			}
			else if(symbol==256)	// END OF BLOCK
			{
				if(m_bFinalBlock)
					state = STATE_COMPLETED;
				else
					state = STATE_START;
				break;
			}
			else if(symbol<286)
			{
				state = STATE_GET_LEN;
			}
			else
				DOERR(XINFLATE_ERR_INVALID_SYMBOL);		// �߻� �Ұ�

			break;

			// ���̳��� ������ ����
		case STATE_DYNAMIC_HUFFMAN :
			if(bitLen<5+5+4) 
				goto END;

			// ���̺� �ʱ�ȭ
			SAFE_DEL(m_pDynamicInfTable);
			SAFE_DEL(m_pDynamicDistTable);

			m_literalsNum  = 257 + BS_EATBITS(5);		// �ִ� 288 (257+11111)
			m_distancesNum = 1   + BS_EATBITS(5);		// �ִ� 32
			m_lenghtsNum   = 4   + BS_EATBITS(4);

			if(m_literalsNum > 286 || m_distancesNum > 30)
				DOERR(XINFLATE_ERR_INVALID_LEN);

			memset(m_lengths, 0, sizeof(m_lengths));
			m_lenghtsPtr = 0;

			state = STATE_DYNAMIC_HUFFMAN_LENLEN ;
			break;

			// ������ ���� ��������.
		case STATE_DYNAMIC_HUFFMAN_LENLEN :

			if(bitLen<3) 
				goto END;

			// 3bit �� �ڵ� ������ �ڵ� ���� ���� ��������.
			while (m_lenghtsPtr < m_lenghtsNum && bitLen >= 3) 
			{
				if(m_lenghtsPtr>sizeof(lenlenmap))						// �Է°� üũ..
					DOERR(XINFLATE_ERR_INVALID_LEN);
#ifdef _DEBUG
				if(lenlenmap[m_lenghtsPtr]>=286+32) ASSERT(0);
				if(m_lenghtsPtr>=sizeof(lenlenmap)) ASSERT(0);
#endif

				m_lengths[lenlenmap[m_lenghtsPtr]] = BS_EATBITS(3);
				m_lenghtsPtr++;
			}

			// �� ����������..
			if (m_lenghtsPtr == m_lenghtsNum)
			{
				// ���̿� ���� ������ ���̺� �����
				if(CreateTable(m_lengths, 19, m_pLenLenTable, err)==FALSE)
					DOERR(err);
				state = STATE_DYNAMIC_HUFFMAN_LEN;
				m_lenghtsPtr = 0;
			}
			break;

			// ����� ���� ������ m_pLenLenTable �� ���ļ� ��������.
		case STATE_DYNAMIC_HUFFMAN_LEN:

			// �� ����������
			if (m_lenghtsPtr >= m_literalsNum + m_distancesNum) 
			{
				// ���� ���̳��� ���̺� ���� (literal + distance)
				if(	CreateTable(m_lengths, m_literalsNum, m_pDynamicInfTable, err)==FALSE ||
					CreateTable(m_lengths + m_literalsNum, m_distancesNum, m_pDynamicDistTable, err)==FALSE)
					DOERR(err);

				// lenlen ���̺��� ���� ���̻� �ʿ����.
				SAFE_DEL(m_pLenLenTable);

				// ���̺� ����
				m_pCurrentTable = m_pDynamicInfTable;
				m_pCurrentDistTable = m_pDynamicDistTable;

				// ��¥ ���� ���� ����
				state = STATE_GET_SYMBOL;
				break;
			}

			{
				// ���� ���� �ڵ� ��������
				int code=-1;
				HUFFLOOKUP(code, m_pLenLenTable);

				if (code == -1)
					goto END;

				if (code < 16) 
				{
					if(m_lenghtsPtr>sizeof(m_lengths))		// �� üũ
						DOERR(XINFLATE_ERR_INVALID_LEN);

					m_lengths[m_lenghtsPtr] = code;
					m_lenghtsPtr++;
				} 
				else 
				{
					m_lenExtraBits = (code == 16 ? 2 : code == 17 ? 3 : 7);
					m_lenAddOn = (code == 18 ? 11 : 3);
					m_lenRep = (code == 16 && m_lenghtsPtr > 0 ? m_lengths[m_lenghtsPtr - 1] : 0);
					state = STATE_DYNAMIC_HUFFMAN_LENREP;
				}
			}
			break;

		case STATE_DYNAMIC_HUFFMAN_LENREP:
			if (bitLen < m_lenExtraBits)
				goto END;

			{
				int repeat = m_lenAddOn + BS_EATBITS(m_lenExtraBits);

				while (repeat > 0 && m_lenghtsPtr < m_literalsNum + m_distancesNum) 
				{
					m_lengths[m_lenghtsPtr] = m_lenRep;
					m_lenghtsPtr++;
					repeat--;
				}
			}

			state = STATE_DYNAMIC_HUFFMAN_LEN;
			break;

		case STATE_COMPLETED :
			goto END;
			break;

		default :
			ASSERT(0);
		}
	}

END :
	if (stream->Write(windowStartPos, (int)(outBufferCur - windowStartPos)) == FALSE)
		return XINFLATE_ERR_USER_STOP;

	if(state!= STATE_COMPLETED)			// ���� �߸����.
	{ASSERT(0); return XINFLATE_ERR_STREAM_NOT_COMPLETED;}

	if (inBufferRemain) ASSERT(0);

	return ret;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         length ������ ������ code �� �����Ѵ�. 
//          RFC 1951 3.2.2
/// @param  
/// @return 
/// @date   Wednesday, April 07, 2010  1:53:36 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL XInflate::CreateCodes(BYTE* lengths, int numSymbols, int* codes)
{
	int		bits;
	int		code = 0;
	int		bitLenCount[MAX_CODELEN+1];
	int		next_code[MAX_CODELEN+1];
	int		i;
	int		n;
	int		len;

	memset(bitLenCount, 0, sizeof(bitLenCount));
	for(i=0;i<numSymbols;i++)
	{
		bitLenCount[lengths[i]] ++;
	}

	bitLenCount[0] = 0;

	for(bits=1;bits<=MAX_CODELEN;bits++)
	{
		code = (code + bitLenCount[bits-1]) << 1;
		next_code[bits] = code;
	}

	for(n=0; n<numSymbols; n++)
	{
		len = lengths[n];
		if(len!=0)
		{
			codes[n] = next_code[len];
			next_code[len]++;
		}
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         codes + lengths ������ ������ ���̺��� �����.
/// @param  
/// @return 
/// @date   Wednesday, April 07, 2010  3:43:21 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL XInflate::CreateTable(BYTE* lengths, int numSymbols, XFastHuffTable*& pTable, XINFLATE_ERR& err)
{
	int			codes[MAX_SYMBOLS];
	// lengths ������ ������ �ڵ����� codes ������ �����Ѵ�.
	if(CreateCodes(lengths, numSymbols, codes)==FALSE) 
	{	
		err = XINFLATE_ERR_CREATE_CODES;
		ASSERT(0); 
		return FALSE;
	}
	if(_CreateTable(codes, lengths, numSymbols, pTable)==FALSE)
	{
		err = XINFLATE_ERR_CREATE_TABLE;
		ASSERT(0); 
		return FALSE;
	}
	return TRUE;
}
BOOL XInflate::_CreateTable(int* codes, BYTE* lengths, int numSymbols, XFastHuffTable*& pTable)
{
	int		bitLenCount[MAX_CODELEN+1];
	int		symbol;
	int		bitLen;

	// bit length ���ϱ�
	memset(bitLenCount, 0, sizeof(bitLenCount));
	for(symbol=0;symbol<numSymbols;symbol++)
		bitLenCount[lengths[symbol]] ++;


	// ������ Ʈ������ ��ȿ�� �ּ� bitlen �� �ִ� bitlen ���ϱ�
	int	bitLenMax = 0;
	int	bitLenMin = MAX_CODELEN;

	for(bitLen=1;bitLen<=MAX_CODELEN;bitLen++)
	{
		if(bitLenCount[bitLen])
		{
			bitLenMax = max(bitLenMax, bitLen);
			bitLenMin = min(bitLenMin, bitLen);
		}
	}

	// ���̺� ����
	pTable = new XFastHuffTable;
	if(pTable==0) {ASSERT(0); return FALSE;}			// �߻� �Ұ�.
	pTable->Create(bitLenMin, bitLenMax);


	// ���̺� ä���
	for(symbol=0;symbol<numSymbols;symbol++)
	{
		bitLen = lengths[symbol];
		if(bitLen)
		{
			if(pTable->SetValue(symbol, bitLen, codes[symbol])==FALSE)
				return FALSE;
		}
	}

//#ifdef _DEBUG
	// ����Ÿ�� �ջ�� ��� ���̺��� ���������� ��������� ���� �� �����Ƿ� ���� ó�� �ʿ�.
	for(UINT i=0;i<pTable->itemCount;i++)
	{
		if(pTable->pItem[i].symbol==HUFFTABLE_VALUE_NOT_EXIST)
		{
			ASSERT(0);
			return FALSE;
		}
	}
//#endif

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         static �������� ����� ���̺� �����
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:17:06 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
void XInflate::CreateStaticTable()
{
	BYTE		lengths[MAX_SYMBOLS];

	// static symbol ���̺� �����
	// RFC1951 3.2.6
    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 256 - 144);
    memset(lengths + 256, 7, 280 - 256);
    memset(lengths + 280, 8, 288 - 280);

	XINFLATE_ERR err;
	CreateTable(lengths, MAX_SYMBOLS, m_pStaticInfTable, err);

	// static dist ���̺� �����
	// RFC1951 3.2.6
	memset(lengths, 5, 32);
	CreateTable(lengths, 32, m_pStaticDistTable, err);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         �Է� ���۰� ����� ��� ���� ���ڵ��� �����Ѵ�.
/// @param  
/// @return 
/// @date   Thursday, May 13, 2010  1:43:34 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
XINFLATE_ERR XInflate::FastInflate(IDecodeStream* stream, LPBYTE& inBuffer, int& inBufferRemain,
		LPBYTE& outBufferCur, 
		LPBYTE& outBufferEnd,
		LPBYTE& windowStartPos, LPBYTE& windowCurPos,
		UINT&		bits, int& bitLen,
		STATE&		state,
		int& windowLen, int& windowDistCode, int& symbol)
{
	XINFLATE_ERR	ret = XINFLATE_ERR_OK;
	int				extrabits;
	int				dist;
	XFastHuffItem*	pItem;								// HUFFLOOKUP() ���� ���
	XFastHuffItem*	pItems = m_pCurrentTable->pItem;	// HUFFLOOKUP ���� ���� �����Ҽ��ְ� ���� ������ �����س���
	int				mask = m_pCurrentTable->mask;		// ...��������.
	BYTE			byte;								// �ӽ� ����

	// ���� ���鼭 ���� ����
	while(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)
	{
		FILLBUFFER_FAST();

		/////////////////////////////////////
		// ���� ��������
		extrabits = lencodes_extrabits[symbol - 257];

		// RFC 1951 3.2.5
		// �⺻ ���̿� extrabit ��ŭ�� ��Ʈ�� ������ ���ϸ� ��¥ ���̰� ���´�
		windowLen = lencodes_min[symbol - 257] + BS_EATBITS(extrabits);

		FILLBUFFER_FAST();

		
		/////////////////////////////////////
		// �Ÿ� �ڵ� ��������
		HUFFLOOKUP(windowDistCode, m_pCurrentDistTable);

		//if(windowDistCode==30 || windowDistCode==31)	// 30 �� 31�� ���� �� ����. RFC1951 3.2.6
		if(windowDistCode>=30)							// �̷л� 32, 33 � ���� �� �ִµ�?
			DOERR(XINFLATE_ERR_INVALID_DIST);
		if(windowDistCode<0)
			DOERR(XINFLATE_ERR_INVALID_DIST);											// fast inflate ����..

		FILLBUFFER_FAST();


		/////////////////////////////////////
		// �Ÿ� ��������
		
		//rec = &distcodes[windowDistCode];
		extrabits = distcodes_extrabits[windowDistCode];

		// DIST ���ϱ�
		dist = distcodes_min[windowDistCode] + BS_EATBITS(extrabits);


		/////////////////////////////////////
		// lz77 ���
		if(outBufferEnd - outBufferCur > windowLen)
		{
			// ��� ���۰� ����� ��� - ��� ���۰� ������� ���θ� üũ���� �ʴ´�.
			do
			{
				byte = *WIN_GETBUF(dist);
				WRITE_FAST(byte);
			}while(--windowLen);
		}
		else
		{
			// ��� ���۰� ������� ���� ���
			do
			{
				byte = *WIN_GETBUF(dist);
				WRITE(byte);
			}while(--windowLen);
		}


		/////////////////////////////////////
		// �ɺ� ��������.
		for(;;)
		{
			// �Է� ���� üũ
			if(!(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)) 
			{
				state = STATE_GET_SYMBOL;
				goto END;
			}

			FILLBUFFER_FAST();

			HUFFLOOKUP_FAST(symbol);

			if(symbol<0) 
			{ASSERT(0); goto END;}					// �߻� �Ұ�.
			else if(symbol<256)
			{
				byte = (BYTE)symbol;
				WRITE(byte);
			}
			else if(symbol==256)	// END OF BLOCK
			{
				if(m_bFinalBlock)
					state = STATE_COMPLETED;
				else
					state = STATE_START;
				// �Լ� ����
				goto END;
			}
			else if(symbol<286)
			{
				// ���� ��������� �����Ѵ�.
				state = STATE_GET_LEN;
				break;
			}
			else
				DOERR(XINFLATE_ERR_INVALID_SYMBOL);		// �߻� �Ұ�
		}
	}

END :
	return ret;
}

