////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// 
/// 
/// @author   park
/// @date     07/22/16 11:19:22
/// 
/// Copyright(C) 2016 Bandisoft, All rights reserved.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once


CString AddPath(CString sLeft, CString sRight, CString sPathChar=L"\\");

BOOL	DigPath(CString sFolderPath);

CString GetParentPath(const CString& sPathName);