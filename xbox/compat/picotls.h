#pragma once

/* NXDK's clang driver identifies as MSVC and does not publish __GNUC__, but
 * picotls uses that macro only to enable compiler builtins clang supports. */
#ifndef __GNUC__
#define NXDK_UNDEFINE_GNUC_AFTER_PICOTLS
#define __GNUC__ 4
#endif

#include_next <picotls.h>

#ifdef NXDK_UNDEFINE_GNUC_AFTER_PICOTLS
#undef __GNUC__
#undef NXDK_UNDEFINE_GNUC_AFTER_PICOTLS
#endif
