#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "assert.h"

#include "typedefs.c"
#include "arena.c"

#include <Windows.h>

#define CASE_PRINT_ENUM(enum) case enum: printf(#enum"\n");
#define READ_BIG_ENDIAN_16(p) (u16)((((u8*)(p))[0] << 8) | (((u8*)(p))[1]))
#define READ_BIG_ENDIAN_32(p) (u32)((((u8*)(p))[0] << 24) | (((u8*)(p))[1] << 16) | (((u8*)(p))[2] << 8) | (((u8*)(p))[3]))
#define PTR_MOVE(p, a) ((p) += (a))
#define READ_BIG_ENDIAN_16_MOVE(p) (READ_BIG_ENDIAN_16((p))); (PTR_MOVE((p), 2))
#define READ_BIG_ENDIAN_32_MOVE(p) (READ_BIG_ENDIAN_32((p))); (PTR_MOVE((p), 4))

typedef enum {
  TRUETYPE    = 0x00010000, // The font contains TrueType outlines.
  TRUETYPE_EX = 0x74727565,
  OPENTYPE    = 0x4F54544F, // The font contains Compact Font Format data (version 1 or 2), structured like in Adobe Tech Note 5176 & 5177.
  POSTSCRIPT  = 0x74797031,
} ScalableFontType;

typedef union {
  char string[4];
  u32 value;
} Tag;

typedef struct {
  // Table tags are the names given to tables in the OpenType font file.
  Tag tag;
  u32 checksum;
  u32 offset;
  u32 length;
} TableRecord;

/*
  NOTE:
  The table directory format allows for a large number of tables.
  To assist in quick binary searches, the searchRange, entrySelector and rangeShift fields are included as parameters that may be used in configuring search algorithms.
  Binary search is optimal when the number of entries is a power of two.
  Values are multiplied by 16, which is the size of each TableRecord.
*/
typedef struct {
  ScalableFontType scalableFontType;
  u16 numTables;
  u16 searchRange; // = (2^floor(log2(numbTables))) * 16; Provides the largest number of items that can be searched with that constraint.
  u16 entrySelector; // = log2(searchRangee / 16); Indicates the maximum number of levels into the binary tree will need to be entered.
  u16 rangeShift; // = numTables * 16 - searchRange; Provides the remaining number of items that would also need to be searched.
  TableRecord *tableRecords; // Must be sorted in ascending order by tag (case-sensitive).
} TableDirectory;

typedef struct {
  Tag ttcTag;
  u16 majorVersion; // = 1
  u16 minorVersion; // = 0
  u32 numFonts;
  u32 *tableDirectoryOffsets; // size = numFonts * sizeof(u32 *)
} TrueTypeCollectionHeaderV1;

typedef struct {
  Tag ttcTag;
  u16 majorVersion; // = 2
  u16 minorVersion; // = 0
  u32 numFonts;
  u32 *tableDirectoryOffsets; // size = numFonts * sizeof(u32 *)
  u32 dsigTag; // = 0x044534947 or 'DSIG'; Tag indicating that a DSIG table exists. Null if no signature.
  u32 dsigLength; // Null if no signature.
  u32 dsigOffset; // Null if no signature.
} TrueTypeCollectionHeaderV2;

typedef enum {
  UNICODE_ENCODING,
  MACINTOSH_ENCODING,
  ISO_ENCODING, //WARNING: Deprecated.
  MICROSOFT_ENCODING,
  CUSTOM_ENCODING,
} EncodingPlatformID;

typedef enum {
  UNICODE_ENCODING_V1, //WARNING: Deprecated.
  UNICODE_ENCODING_V1_1, //WARNING: Deprecated.
  ISO_IEC_10646, //WARNING: Deprecated.
  UNICODE_ENCODING_V2_BMP_ONLY, // For use with subtable format 4 or 6.
  UNICODE_ENCODING_V2_FULL, // For use with subtable format 10 or 12.
  UNICODE_ENCODING_VARIATION_SEQUENCES, // For use with subtable format 14.
  UNICODE_FULL, // For use with subtable format 13.
} UnicodeEncodingPlatformSpecificID; // platformID = 0

typedef enum {
  ROMAN_ENCODING,
  JAPANESE_ENCODING,
  CHINESE_TRADITIONAL_ENCODING,
  KOREAN_ENCODING,
  ARABIC_ENCODING,
  HEBREW_ENCODING,
  GREEK_ENCODING,
  RUSSIAN_ENCODING,
  RSYMBOL_ENCODING,
  DEVANAGARI_ENCODING,
  GURMUKHI_ENCODING,
  GUJARATI_ENCODING,
  ODIA_ENCODING,
  BANGLA_ENCODING,
  TAMIL_ENCODING,
  TELUGU_ENCODING,
  KANNADA_ENCODING,
  MALAYALAM_ENCODING,
  SINHALESE_ENCODING,
  BURMESE_ENCODING,
  KHMER_ENCODING,
  THAI_ENCODING,
  LAOTIAN_ENCODING,
  GEORGIAN_ENCODING,
  ARMENIAN_ENCODING,
  CHINESE_SIMPLIFIED_ENCODING,
  TIBETAN_ENCODING,
  MOGOLIAN_ENCODING,
  GEEZ_ENCODING,
  SLAVIC_ENCODING,
  VIETNAMESE_ENCODING,
  SINDHI_ENCODING,
  UNINTERPRETED_ENCODING,
} MacintoshEncodingPlatformSpecificID; // platformID = 1

typedef enum {
  SYMBOL_ENCODING,
  UNICODE_BMP_ENCODING, // For use with subtable format 4. WARNING: Must not be used to support Unicode supplementary-plane characters.
  SHIFTJIS_ENCODING,
  PRC_ENCODING,
  BIG5_ENCODING,
  WANSUNG_ENCODING,
  JOHAB_ENCODING,
  UNICODE_FULL_ENCODING = 10,
} WindowsEncodingPlatformSpecificID; // platformID = 3

typedef struct {
  union {
    EncodingPlatformID _;
    u16 value;
  } platformID;
  union {
    UnicodeEncodingPlatformSpecificID unicode;
    MacintoshEncodingPlatformSpecificID macintosh;
    WindowsEncodingPlatformSpecificID windows;
    u16 value;
  } platformSpecificID;
  u32 offset;
} EncodingRecord;

typedef struct {
  u16 version;
  u16 numTables;
  EncodingRecord *encodingRecords;
} CodepointMapTable; // 'cmap' table

typedef struct {
  u16 format; // = 0
  u16 length;
  u16 language;
  u8 glyphIdArray[256];
} CodepointMapFormat0; // Used on older Macintosh platforms but not required on newer Apple platforms.

typedef struct {
  u16 firstCode; // First valid low byte for this SubHeader.
  u16 entryCount; // Number of valid low bytes for this SubHeader starting from firstCode.
  i16 idDelta; // Added to the glyph ID read from glyphIdArray to compute the final glyph index. Is modulo 65536.
  u16 idRangeOffset; // Offset from the location of this field to the glyphIdArray entry corresponding to firstCode.
} SubHeader;

typedef struct {
  u16 format; // = 2
  u16 length;
  u16 language;
  u16 subHeaderKeys[256]; // [= subHeaders index * 16]; Maps high bytes into subHeaders.
  SubHeader *subHeaders;
} CodepointMapFormat2; // For double-byte encodings following national character code standards used for Japanese, Chinese and Korean chars.

typedef struct {
  u16 format; // = 4
  u16 length;
  u16 language;
  u16 segmentsCountX2;
  u16 searchRange; // = (2^floor(log2(numbTables))) * 16; Provides the largest number of items that can be searched with that constraint.
  u16 entrySelector; // = log2(searchRangee / 16); Indicates the maximum number of levels into the binary tree will need to be entered.
  u16 rangeShift; // = numTables * 16 - searchRange; Provides the remaining number of items that would also need to be searched.
  u16 *endCode; // End characterCode for each segment, last=0xFFFF.
  u16 reservedPad; // = 0
  u16 *startCode; // Start character code for each segment.
  u16 *idDelta; // Delta for all character codes in segment.
  u16 *idRangeOffset;
  u16 *glyphIdArray;
} CodepointMapFormat4; // For fonts that support only Unicode Basic Multilingual Plane characters (U+0000 to U+FFFF).
/*
  WARNING:
  In early implementations on devices with limited hardware capabilities, optimizations provided by the searchRange, entrySelector and
  rangeShift fields were of high importance. They have less importance on modern devices but could still be used in some implementations.
  However, incorrect values could potentially be used as an attack vector against some implementations. Since these values can be derived
  from the segCountX2 field when the file is parsed, it is strongly recommended that parsing implementations not rely on the searchRange,
  entrySelector and rangeShift fields in the font but derive them independently from segCountX2. Font files, however, should continue to
  provide valid values for these fields to maintain compatibility with all existing implementations.
*/

//TODO: Implement/Update ReadFormat... functions according to the spec: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#cmap-subtable-formats

TableDirectory *ReadTableDirectory(Arena *arena, char *buffer)
{
  char *pBuffer = buffer;
  
  TableDirectory *fontDirectory = (TableDirectory  *)Alloc(arena, sizeof(TableDirectory));
  fontDirectory->scalableFontType = READ_BIG_ENDIAN_32_MOVE(pBuffer);
  fontDirectory->numTables = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  fontDirectory->searchRange = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  fontDirectory->entrySelector = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  fontDirectory->rangeShift = READ_BIG_ENDIAN_16_MOVE(pBuffer);

  i32 numTables = fontDirectory->numTables;
  fontDirectory->tableRecords = (TableRecord *)Alloc(arena, numTables * sizeof(TableRecord));  
  for (i32 i = 0; i < numTables; ++i)
  {
    TableRecord *tableRecord = &fontDirectory->tableRecords[i];
    tableRecord->tag.value = READ_BIG_ENDIAN_32_MOVE(pBuffer);
    tableRecord->checksum = READ_BIG_ENDIAN_32_MOVE(pBuffer);
    tableRecord->offset = READ_BIG_ENDIAN_32_MOVE(pBuffer);
    tableRecord->length = READ_BIG_ENDIAN_32_MOVE(pBuffer);
  }

  buffer = pBuffer;
  return fontDirectory;
}

CodepointMapTable *ReadCodepointMapTable(Arena *arena, char *buffer)
{
  char *pBuffer = buffer;
  
  CodepointMapTable *codepointMapTable = (CodepointMapTable *)Alloc(arena, sizeof(CodepointMapTable));
  codepointMapTable->version = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  codepointMapTable->numTables = READ_BIG_ENDIAN_16_MOVE(pBuffer);

  i32 numTables = codepointMapTable->numTables;
  codepointMapTable->encodingRecords = (EncodingRecord *)Alloc(arena, numTables * sizeof(EncodingRecord));
  for (i32 i = 0; i < numTables; ++i)
  {
    EncodingRecord *encodingRecord = &codepointMapTable->encodingRecords[i];
    encodingRecord->platformID.value = READ_BIG_ENDIAN_16_MOVE(pBuffer);
    encodingRecord->platformSpecificID.value = READ_BIG_ENDIAN_16_MOVE(pBuffer);
    encodingRecord->offset = READ_BIG_ENDIAN_32_MOVE(pBuffer);
  }

  buffer = pBuffer;
  return codepointMapTable;
}

CodepointMapFormat4 *ReadCodepointMapFormat4(Arena *arena, char *buffer)
{
  char *pBuffer = buffer;
  
  u16 length = READ_BIG_ENDIAN_16(&pBuffer[2]);
  CodepointMapFormat4 *format4 = (CodepointMapFormat4 *)Alloc(arena, length + 5 * sizeof(u16 *));
  
  //TODO: SIMD
  format4->format = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  format4->length = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  format4->language = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  format4->segmentsCountX2 = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  format4->searchRange = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  format4->entrySelector = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  format4->rangeShift = READ_BIG_ENDIAN_16_MOVE(pBuffer);

  u16 segmentsCount = format4->segmentsCountX2 / 2;
  format4->endCode = (u16 *)((u8 *)format4 + sizeof(format4));
  format4->startCode = format4->endCode + segmentsCount;
  format4->idDelta = format4->startCode + segmentsCount;
  format4->idRangeOffset = format4->idDelta + segmentsCount;
  format4->glyphIdArray = format4->idRangeOffset + segmentsCount;

  char *startCodeStart = &pBuffer[segmentsCount + 2];
  char *idDeltaStart = &pBuffer[segmentsCount * 2 + 2];
  char *idRangeStart = &pBuffer[segmentsCount * 3 + 2];
  for (i32 i = 0; i < segmentsCount; ++i)
  {
    i32 offset = i * 2;
    format4->endCode[i] = READ_BIG_ENDIAN_16(&pBuffer[offset]);
    format4->startCode[i] = READ_BIG_ENDIAN_16(&startCodeStart[offset]);
    format4->idDelta[i] = READ_BIG_ENDIAN_16(&idDeltaStart[offset]);
    format4->idRangeOffset[i] = READ_BIG_ENDIAN_16(&idRangeStart[offset]);
  }

  PTR_MOVE(pBuffer, format4->segmentsCountX2 * 4 + 2);

  i64 halfRemainingBytes = (format4->length - (pBuffer - buffer)) / 2;
  for (i32 i = 0; i < halfRemainingBytes; ++i)
  {
    format4->glyphIdArray[i] = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  }

  return format4;
}

#if DEBUG
void PrintTableDirectory(TableDirectory *fontDirectory)
{
  printf("-- Font directory\nScalable font type: ");
  switch (fontDirectory->scalableFontType)
  {
    CASE_PRINT_ENUM(TRUETYPE);
    CASE_PRINT_ENUM(OPENTYPE);
    CASE_PRINT_ENUM(POSTSCRIPT);
    default: printf("\n"); break;
  }
  
  TableRecord *tableRecords = fontDirectory->tableRecords;
  i32 numTables = fontDirectory->numTables;
  for (i32 i = 0; i < numTables; ++i)
  {
    TableRecord *tableRecord = &tableRecords[i];
    printf("%c%c%c%c %d %d\n",
           tableRecord->tag.string[3],
           tableRecord->tag.string[2],
           tableRecord->tag.string[1],
           tableRecord->tag.string[0],
           tableRecord->length,
           tableRecord->offset);
  }
}

void PrintCodepointMapTable(CodepointMapTable *codepointMapTable)
{
  printf("-- Codepoint map\n");
  i32 numTables = codepointMapTable->numTables;
	for (i32 i = 0; i < numTables; ++i)
	{
		EncodingRecord* encodingRecord = &codepointMapTable->encodingRecords[i];
		printf("%d:\n  platformID: ", i);
		switch (encodingRecord->platformID.value)
		{
      case UNICODE_ENCODING: {
    		printf("UNICODE_ENCODING\n  platformSpecificID: ");
    		switch(encodingRecord->platformSpecificID.unicode)
    		{
          CASE_PRINT_ENUM(UNICODE_ENCODING_V1); break;
          CASE_PRINT_ENUM(UNICODE_ENCODING_V1_1); break;
          CASE_PRINT_ENUM(ISO_IEC_10646); break;
          CASE_PRINT_ENUM(UNICODE_ENCODING_V2_BMP_ONLY); break;
          CASE_PRINT_ENUM(UNICODE_ENCODING_V2_FULL); break;
          CASE_PRINT_ENUM(UNICODE_ENCODING_VARIATION_SEQUENCES); break;
          CASE_PRINT_ENUM(UNICODE_FULL);
    			default: fprintf(stderr, "Unrecognizable platform specific id"); break;
    		}
      } break;
      
      case MACINTOSH_ENCODING: {
    		printf("MACINTOSH_ENCODING\n  platformSpecificID: ");
    		switch(encodingRecord->platformSpecificID.macintosh)
    		{
          CASE_PRINT_ENUM(ROMAN_ENCODING); break;
          CASE_PRINT_ENUM(JAPANESE_ENCODING); break;
          CASE_PRINT_ENUM(CHINESE_TRADITIONAL_ENCODING); break;
          CASE_PRINT_ENUM(KOREAN_ENCODING); break;
          CASE_PRINT_ENUM(ARABIC_ENCODING); break;
          CASE_PRINT_ENUM(HEBREW_ENCODING); break;
          CASE_PRINT_ENUM(GREEK_ENCODING); break;
          CASE_PRINT_ENUM(RUSSIAN_ENCODING); break;
          CASE_PRINT_ENUM(RSYMBOL_ENCODING); break;
          CASE_PRINT_ENUM(DEVANAGARI_ENCODING); break;
          CASE_PRINT_ENUM(GURMUKHI_ENCODING); break;
          CASE_PRINT_ENUM(GUJARATI_ENCODING); break;
          CASE_PRINT_ENUM(ODIA_ENCODING); break;
          CASE_PRINT_ENUM(BANGLA_ENCODING); break;
          CASE_PRINT_ENUM(TAMIL_ENCODING); break;
          CASE_PRINT_ENUM(TELUGU_ENCODING); break;
          CASE_PRINT_ENUM(KANNADA_ENCODING); break;
          CASE_PRINT_ENUM(MALAYALAM_ENCODING); break;
          CASE_PRINT_ENUM(SINHALESE_ENCODING); break;
          CASE_PRINT_ENUM(BURMESE_ENCODING); break;
          CASE_PRINT_ENUM(KHMER_ENCODING); break;
          CASE_PRINT_ENUM(THAI_ENCODING); break;
          CASE_PRINT_ENUM(LAOTIAN_ENCODING); break;
          CASE_PRINT_ENUM(GEORGIAN_ENCODING); break;
          CASE_PRINT_ENUM(ARMENIAN_ENCODING); break;
          CASE_PRINT_ENUM(CHINESE_SIMPLIFIED_ENCODING); break;
          CASE_PRINT_ENUM(TIBETAN_ENCODING); break;
          CASE_PRINT_ENUM(MOGOLIAN_ENCODING); break;
          CASE_PRINT_ENUM(GEEZ_ENCODING); break;
          CASE_PRINT_ENUM(SLAVIC_ENCODING); break;
          CASE_PRINT_ENUM(VIETNAMESE_ENCODING); break;
          CASE_PRINT_ENUM(SINDHI_ENCODING); break;
          CASE_PRINT_ENUM(UNINTERPRETED_ENCODING); break;
    			default: fprintf(stderr, "Unrecognizable platform specific id"); break;
    		}
      } break;
        
      case MICROSOFT_ENCODING: {
    		printf("MICROSOFT_ENCODING\n  platformSpecificID: ");
    		switch(encodingRecord->platformSpecificID.windows)
    		{
          CASE_PRINT_ENUM(SYMBOL_ENCODING); break;
          CASE_PRINT_ENUM(UNICODE_BMP_ENCODING); break;
          CASE_PRINT_ENUM(SHIFTJIS_ENCODING); break;
          CASE_PRINT_ENUM(PRC_ENCODING); break;
          CASE_PRINT_ENUM(BIG5_ENCODING); break;
          CASE_PRINT_ENUM(WANSUNG_ENCODING); break;
          CASE_PRINT_ENUM(JOHAB_ENCODING); break;
          CASE_PRINT_ENUM(UNICODE_FULL_ENCODING); break;
    			default: fprintf(stderr, "Unrecognizable platform specific id"); break;
    		}
      } break;
      
      case ISO_ENCODING:
      case CUSTOM_ENCODING:
			default: fprintf(stderr, "Unsupported platform\n"); break;
		}
		
		printf("  offset: %d\n", encodingRecord->offset);
	}
}

void PrintCodepointMapFormat4(CodepointMapFormat4 *format4)
{
  i32 segmentsCount = format4->segmentsCountX2 / 2;
  printf("-- Format: %d\nlength: %d\nlanguage: %d\nsegmentsCount: %d\nsearchRange: %d\nentrySelector: %d\nrangeShift: %d\nSegment ranges:\n",
         format4->format,
         format4->length,
         format4->language,
         segmentsCount,
      	 format4->searchRange,
      	 format4->entrySelector,
      	 format4->rangeShift);
    
  for (i32 i = 0; i < segmentsCount; ++i)
  {
  	printf("[%d]: startCode: %9d endCode: %7d idDelta: %7d idRangeOffset: %12d\n",
  	       i,
  	       format4->startCode[i],
  	       format4->endCode[i],
  	       format4->idDelta[i],
  	       format4->idRangeOffset[i]);
  }
}
#endif

char *ReadWholeFile(Arena *arena, char *filePath, size_t *fileSize)
{
  FILE *file = fopen(filePath, "rb");
  if (file)
  {
    fseek(file, 0, SEEK_END);
    *fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *content = (char *)Alloc(arena, *fileSize + 1);
    i32 success = fread(content, *fileSize, 1, file);
    fclose(file);
    return success ? content : NULL;
  }
  return NULL;
}

int main()
{
  Arena arena;
  InitArena(&arena, VirtualAlloc(NULL, 2*GB, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE), 2*GB);

  size_t fileSize = 0;
  char *buffer = ReadWholeFile(&arena, "NotoSans.ttf", &fileSize);
  if (buffer)
  {
    TableDirectory *fontDirectory = ReadTableDirectory(&arena, buffer);
#if DEBUG
    PrintTableDirectory(fontDirectory);
#endif
    i32 numTables = fontDirectory->numTables;
    for (i32 i = 0; i < numTables; ++i)
    {
      if (fontDirectory->tableRecords[i].tag.value == READ_BIG_ENDIAN_32("cmap"))
      {
        char *pBuffer = &buffer[fontDirectory->tableRecords[i].offset];
        CodepointMapTable *codepointMapTable = ReadCodepointMapTable(&arena, pBuffer);
#if DEBUG
        PrintCodepointMapTable(codepointMapTable);
#endif
        break;
      }
    }

    CodepointMapFormat4 *format4 = ReadCodepointMapFormat4(&arena, buffer);
    PrintCodepointMapFormat4(format4);
  }
  
  return 0;
}
