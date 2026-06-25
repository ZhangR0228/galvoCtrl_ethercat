/*
* This source file is part of the EtherCAT Slave Stack Code licensed by Beckhoff Automation GmbH & Co KG, 33415 Verl, Germany.
* The corresponding license agreement applies. This hint shall not be removed.
*/

/**
* \addtogroup SSC-Device SSC-Device
* @{
*/
#include "ecatappl.h"
/**
\file SSC-DeviceObjects
\author ET9300Utilities.ApplicationHandler (Version 1.5.0.0) | EthercatSSC@beckhoff.com

\brief SSC-Device specific objects<br>
\brief NOTE : This file will be overwritten if a new object dictionary is generated!<br>
*/

#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
#define PROTO
#else
#define PROTO extern
#endif
/******************************************************************************
*                    Object 0x1601 : Output mapping 1
******************************************************************************/
#ifdef _OBJD_
OBJCONST TSDOINFOENTRYDESC OBJMEM asEntryDesc0x1601[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ }};

OBJCONST UCHAR OBJMEM aName0x1601[] = "Output mapping 1\000"
"SubIndex 001\000"
"SubIndex 002\000"
"SubIndex 003\000"
"SubIndex 004\000\377";
#endif

#ifndef _SSC_DEVICE_OBJECTS_H_
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT32 SI1;
UINT32 SI2;
UINT32 SI3;
UINT32 SI4;
} OBJ_STRUCT_PACKED_END
TOBJ1601;
#endif

PROTO TOBJ1601 OutputMapping10x1601
#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
={4,0x70100110,0x70100210,0x70100310,0x70100410}
#endif
;

/******************************************************************************
*                    Object 0x1A00 : Input mapping 0
******************************************************************************/
#ifdef _OBJD_
OBJCONST TSDOINFOENTRYDESC OBJMEM asEntryDesc0x1A00[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ }};

OBJCONST UCHAR OBJMEM aName0x1A00[] = "Input mapping 0\000"
"SubIndex 001\000"
"SubIndex 002\000"
"SubIndex 003\000\377";
#endif

#ifndef _SSC_DEVICE_OBJECTS_H_
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT32 SI1;
UINT32 SI2;
UINT32 SI3;
} OBJ_STRUCT_PACKED_END
TOBJ1A00;
#endif

PROTO TOBJ1A00 InputMapping00x1A00
#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
={3,0x60000110,0x60000210,0x60000310}
#endif
;

/******************************************************************************
*                    Object 0x1C12 : SyncManager 2 assignment
******************************************************************************/
#ifdef _OBJD_
OBJCONST TSDOINFOENTRYDESC OBJMEM asEntryDesc0x1C12[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }};

OBJCONST UCHAR OBJMEM aName0x1C12[] = "SyncManager 2 assignment\000\377";
#endif

#ifndef _SSC_DEVICE_OBJECTS_H_
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT16 aEntries[1];
} OBJ_STRUCT_PACKED_END
TOBJ1C12;
#endif

PROTO TOBJ1C12 sRxPDOassign
#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
={1,{0x1601}}
#endif
;

/******************************************************************************
*                    Object 0x1C13 : SyncManager 3 assignment
******************************************************************************/
#ifdef _OBJD_
OBJCONST TSDOINFOENTRYDESC OBJMEM asEntryDesc0x1C13[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }};

OBJCONST UCHAR OBJMEM aName0x1C13[] = "SyncManager 3 assignment\000\377";
#endif

#ifndef _SSC_DEVICE_OBJECTS_H_
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT16 aEntries[1];
} OBJ_STRUCT_PACKED_END
TOBJ1C13;
#endif

PROTO TOBJ1C13 sTxPDOassign
#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
={1,{0x1A00}}
#endif
;

/******************************************************************************
*                    Object 0x6000 : Galvo status
******************************************************************************/
#ifdef _OBJD_
OBJCONST TSDOINFOENTRYDESC OBJMEM asEntryDesc0x6000[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }};

OBJCONST UCHAR OBJMEM aName0x6000[] = "Galvo status\000"
"status\000"
"last_sequence\000"
"frame_counter\000\377";
#endif

#ifndef _SSC_DEVICE_OBJECTS_H_
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT16 Status;
UINT16 LastSequence;
UINT16 FrameCounter;
} OBJ_STRUCT_PACKED_END
TOBJ6000;
#endif

PROTO TOBJ6000 Obj0x6000
#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
={3,0,0,0}
#endif
;

/******************************************************************************
*                    Object 0x7010 : Galvo command
******************************************************************************/
#ifdef _OBJD_
OBJCONST TSDOINFOENTRYDESC OBJMEM asEntryDesc0x7010[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_INTEGER16 , 0x10 , ACCESS_READ },
{ DEFTYPE_INTEGER16 , 0x10 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }};

OBJCONST UCHAR OBJMEM aName0x7010[] = "Galvo command\000"
"galvo_x_code\000"
"galvo_y_code\000"
"sequence\000"
"control_flags\000\377";
#endif

#ifndef _SSC_DEVICE_OBJECTS_H_
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
short GalvoXCode;
short GalvoYCode;
UINT16 Sequence;
UINT16 ControlFlags;
} OBJ_STRUCT_PACKED_END
TOBJ7010;
#endif

PROTO TOBJ7010 Obj0x7010
#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
={4,0,0,0,0}
#endif
;

/******************************************************************************
*                    Object 0xF000 : Modular Device Profile
******************************************************************************/
#ifdef _OBJD_
OBJCONST TSDOINFOENTRYDESC OBJMEM asEntryDesc0xF000[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }};

OBJCONST UCHAR OBJMEM aName0xF000[] = "Modular Device Profile\000"
"Index distance \000"
"Maximum number of modules \000\377";
#endif

#ifndef _SSC_DEVICE_OBJECTS_H_
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT16 IndexDistance;
UINT16 MaximumNumberOfModules;
} OBJ_STRUCT_PACKED_END
TOBJF000;
#endif

PROTO TOBJF000 ModularDeviceProfile0xF000
#if defined(_SSC_DEVICE_) && (_SSC_DEVICE_ == 1)
={2,0x0010,0}
#endif
;

#ifdef _OBJD_
TOBJECT OBJMEM ApplicationObjDic[] = {
{NULL , NULL ,  0x1601 , {DEFTYPE_PDOMAPPING , 4 | (OBJCODE_REC << 8)} , asEntryDesc0x1601 , aName0x1601 , &OutputMapping10x1601 , NULL , NULL , 0x0000 },
{NULL , NULL ,  0x1A00 , {DEFTYPE_PDOMAPPING , 3 | (OBJCODE_REC << 8)} , asEntryDesc0x1A00 , aName0x1A00 , &InputMapping00x1A00 , NULL , NULL , 0x0000 },
{NULL , NULL ,  0x1C12 , {DEFTYPE_UNSIGNED16 , 1 | (OBJCODE_ARR << 8)} , asEntryDesc0x1C12 , aName0x1C12 , &sRxPDOassign , NULL , NULL , 0x0000 },
{NULL , NULL ,  0x1C13 , {DEFTYPE_UNSIGNED16 , 1 | (OBJCODE_ARR << 8)} , asEntryDesc0x1C13 , aName0x1C13 , &sTxPDOassign , NULL , NULL , 0x0000 },
{NULL , NULL ,  0x6000 , {DEFTYPE_RECORD , 3 | (OBJCODE_REC << 8)} , asEntryDesc0x6000 , aName0x6000 , &Obj0x6000 , NULL , NULL , 0x0000 },
{NULL , NULL ,  0x7010 , {DEFTYPE_RECORD , 4 | (OBJCODE_REC << 8)} , asEntryDesc0x7010 , aName0x7010 , &Obj0x7010 , NULL , NULL , 0x0000 },
{NULL , NULL ,  0xF000 , {DEFTYPE_RECORD , 2 | (OBJCODE_REC << 8)} , asEntryDesc0xF000 , aName0xF000 , &ModularDeviceProfile0xF000 , NULL , NULL , 0x0000 },
{NULL,NULL, 0xFFFF, {0, 0}, NULL, NULL, NULL, NULL}};
#endif
#undef PROTO

/** @}*/
#define _SSC_DEVICE_OBJECTS_H_
