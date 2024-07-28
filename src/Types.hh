#pragma once

#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/JSON.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>

using le_uint16_t = phosg::le_uint16_t;
using le_int16_t = phosg::le_int16_t;
using le_uint32_t = phosg::le_uint32_t;
using le_int32_t = phosg::le_int32_t;
using le_uint64_t = phosg::le_uint64_t;
using le_int64_t = phosg::le_int64_t;
using le_float = phosg::le_float;
using le_double = phosg::le_double;
using be_uint16_t = phosg::be_uint16_t;
using be_int16_t = phosg::be_int16_t;
using be_uint32_t = phosg::be_uint32_t;
using be_int32_t = phosg::be_int32_t;
using be_uint64_t = phosg::be_uint64_t;
using be_int64_t = phosg::be_int64_t;
using be_float = phosg::be_float;
using be_double = phosg::be_double;

template <bool BE>
using U16T = typename std::conditional<BE, be_uint16_t, le_uint16_t>::type;
template <bool BE>
using S16T = typename std::conditional<BE, be_int16_t, le_int16_t>::type;
template <bool BE>
using U32T = typename std::conditional<BE, be_uint32_t, le_uint32_t>::type;
template <bool BE>
using S32T = typename std::conditional<BE, be_int32_t, le_int32_t>::type;
template <bool BE>
using F32T = typename std::conditional<BE, be_float, le_float>::type;
