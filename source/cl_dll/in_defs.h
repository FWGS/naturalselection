#if !defined( IN_DEFSH )
#define IN_DEFSH
#pragma once

// up / down
#define	PITCH	0
// left / right
#define	YAW		1
// fall over
#define	ROLL	2 
#ifndef _WIN32
typedef struct point_s
{
    int x;
    int y;
} POINT;
#define GetCursorPos(x)
#define SetCursorPos(x,y)
#endif
#endif