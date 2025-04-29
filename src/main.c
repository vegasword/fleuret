#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "assert.h"

#include "typedefs.c"
#include "arena.c"

#include <Windows.h>

#define CASE_PRINT_ENUM(enum) case enum: printf(#enum"\n"); break;
#define READ_BIG_ENDIAN_16(p) (u16)((((u8*)(p))[0] << 8) | (((u8*)(p))[1]))
#define READ_BIG_ENDIAN_32(p) (u32)((((u8*)(p))[0] << 24) | (((u8*)(p))[1] << 16) | (((u8*)(p))[2] << 8) | (((u8*)(p))[3]))
#define PTR_MOVE(p, a) ((p) += (a))
#define READ_BIG_ENDIAN_16_MOVE(p) (READ_BIG_ENDIAN_16((p))); (PTR_MOVE((p), 2))
#define READ_BIG_ENDIAN_32_MOVE(p) (READ_BIG_ENDIAN_32((p))); (PTR_MOVE((p), 4))

typedef enum {
  TRUETYPE_SCALER_TYPE = 0x74727565,
  TRUETYPE_SCALER_TYPE_EX = 0x00010000,
  OPENTYPE_SCALER_TYPE =  0x4F54544F,
  POSTSCRIPT_SCALER_TYPE = 0x74797031,
} ScalerType;

typedef struct {
  ScalerType scalerType;
  u16 tablesCount;
  u16 searchRange;
  u16 entrySelector;
  u16 rangeShift;
} OffsetSubtable;

typedef struct {
  union {
    char string[4];
    u32 value;
  } tag;
  u32 checkSum;
  u32 offset;
  u32 length;
} TableDirectory;

typedef struct {
  OffsetSubtable offsetSubtable;
  TableDirectory *tableDirectories;
} FontDirectory;

typedef enum {
  UNICODE_ENCODING_PID = 0,
  MAC_ENCODING_PID = 1,
  MICROSOFT_ENCODING_PID = 3,
} EncodingPlatformID;

typedef enum {
  UNICODE_ENCODING_V_1 = 0,
  UNICODE_ENCODING_V_1_1 = 1,
  UNICODE_ENCODING_DEPRECATED = 2,
  UNICODE_ENCODING_V_2_BMP_ONLY = 3,
  UNICODE_ENCODING_V_2 = 4,
  UNICODE_ENCODING_VARIATION_SEQUENCES = 5,
} EncodingPlatformSpecificID;

typedef struct {
  EncodingPlatformID platformID;
  EncodingPlatformSpecificID platformSpecificID;
  u32 offset;
} EncodingSubtable;

typedef struct {
  u16 version;
  u16 encodingSubtablesCount;
  EncodingSubtable *encodingSubtables;
} CodepointMap;

FontDirectory *ReadFontDirectory(Arena *arena, char **buffer)
{
  char *pBuffer = *buffer;
  FontDirectory *fontDirectory = (FontDirectory  *)Alloc(arena, sizeof(FontDirectory));
  
  OffsetSubtable *offsetSubtable = &fontDirectory->offsetSubtable;
  offsetSubtable->scalerType = READ_BIG_ENDIAN_32_MOVE(pBuffer);
  offsetSubtable->tablesCount = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  offsetSubtable->searchRange = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  offsetSubtable->entrySelector = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  offsetSubtable->rangeShift = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  
  i32 tablesCount = offsetSubtable->tablesCount;
  fontDirectory->tableDirectories = (TableDirectory *)Alloc(arena, tablesCount * sizeof(TableDirectory));
  for (i32 i = 0; i < tablesCount; ++i)
  {
    TableDirectory *tableDirectory = &fontDirectory->tableDirectories[i];
    tableDirectory->tag.value = READ_BIG_ENDIAN_32_MOVE(pBuffer);
    tableDirectory->checkSum = READ_BIG_ENDIAN_32_MOVE(pBuffer);
    tableDirectory->offset = READ_BIG_ENDIAN_32_MOVE(pBuffer);
    tableDirectory->length = READ_BIG_ENDIAN_32_MOVE(pBuffer);
  }
  
  return fontDirectory;
}

CodepointMap *ReadCodepointMap(Arena *arena, char **buffer)
{
  char *pBuffer = *buffer;
  CodepointMap *codepointMap = (CodepointMap *)Alloc(arena, sizeof(CodepointMap));
  codepointMap->version = READ_BIG_ENDIAN_16_MOVE(pBuffer);
  codepointMap->encodingSubtablesCount = READ_BIG_ENDIAN_16_MOVE(pBuffer);

  i32 encodingSubtablesCount = codepointMap->encodingSubtablesCount;
  codepointMap->encodingSubtables = (EncodingSubtable *)Alloc(arena, encodingSubtablesCount * sizeof(EncodingSubtable));
  for (i32 i = 0; i < encodingSubtablesCount; ++i)
  {
    EncodingSubtable *encodingSubtable = &codepointMap->encodingSubtables[i];
    encodingSubtable->platformID = READ_BIG_ENDIAN_16_MOVE(pBuffer);
    encodingSubtable->platformSpecificID = READ_BIG_ENDIAN_16_MOVE(pBuffer);
    encodingSubtable->offset = READ_BIG_ENDIAN_32_MOVE(pBuffer);
  }

  return codepointMap;
}

#if DEBUG
void PrintFontDirectory(FontDirectory *fontDirectory)
{
  printf("-- Font directory\nScaler type: ");
  switch (fontDirectory->offsetSubtable.scalerType)
  {
    CASE_PRINT_ENUM(TRUETYPE_SCALER_TYPE);
    CASE_PRINT_ENUM(OPENTYPE_SCALER_TYPE);
    CASE_PRINT_ENUM(POSTSCRIPT_SCALER_TYPE);
    default: printf("\n"); break;
  }
  
  TableDirectory *tableDirectories = fontDirectory->tableDirectories;
  i32 tablesCount = fontDirectory->offsetSubtable.tablesCount;
  for (i32 i = 0; i < tablesCount; ++i)
  {
    TableDirectory *tableDirectory = &tableDirectories[i];
    printf("%c%c%c%c %d %d\n",
           tableDirectory->tag.string[3],
           tableDirectory->tag.string[2],
           tableDirectory->tag.string[1],
           tableDirectory->tag.string[0],
           tableDirectory->length,
           tableDirectory->offset);
  }
}

void PrintCodepointMap(CodepointMap *codepointMap)
{
  printf("-- Codepoint map\n");
  i32 encodingSubtablesCount = codepointMap->encodingSubtablesCount;
	for(i32 i = 0; i < encodingSubtablesCount; ++i)
	{
		EncodingSubtable* encodingSubtable = &codepointMap->encodingSubtables[i];
				
		printf("%d:\n  pid: ", i);
		switch(encodingSubtable->platformID)
		{
			CASE_PRINT_ENUM(UNICODE_ENCODING_PID);
			CASE_PRINT_ENUM(MAC_ENCODING_PID);
			CASE_PRINT_ENUM(MICROSOFT_ENCODING_PID);
			default: printf("Not Supported\n"); break;
		}
		
		printf("  psid: ");
		switch(encodingSubtable->platformSpecificID)
		{
			CASE_PRINT_ENUM(UNICODE_ENCODING_V_1);
			CASE_PRINT_ENUM(UNICODE_ENCODING_V_1_1);
			CASE_PRINT_ENUM(UNICODE_ENCODING_DEPRECATED);
		  CASE_PRINT_ENUM(UNICODE_ENCODING_V_2_BMP_ONLY);
	    CASE_PRINT_ENUM(UNICODE_ENCODING_V_2);
			default: printf("Not Supported"); break;
		}
		
		printf("  offset: %d\n", encodingSubtable->offset);
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
  char *buffer = ReadWholeFile(&arena, "envy.ttf", &fileSize);
  if (buffer)
  {
    FontDirectory *fontDirectory = ReadFontDirectory(&arena, &buffer);
#if DEBUG
    PrintFontDirectory(fontDirectory);
#endif
    i32 tablesCount = fontDirectory->offsetSubtable.tablesCount;
    for (i32 i = 0; i < tablesCount; ++i)
    {
      if (fontDirectory->tableDirectories[i].tag.value == READ_BIG_ENDIAN_32("cmap"))
      {
        char *pBuffer = &buffer[fontDirectory->tableDirectories[i].offset];
        CodepointMap *codepointMap = ReadCodepointMap(&arena, &pBuffer);
#if DEBUG
        PrintCodepointMap(codepointMap);
#endif
        break;
      }
    }
  }
  
  return 0;
}
