//
// _RevPosit_h_
//
// Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _REV_POSIT_H_
#define _REV_POSIT_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"

#include "universal/number/posit/posit.hpp"

#pragma GCC diagnostic pop

using posit32 = sw::universal::posit<32, 2>;
using posit64 = sw::universal::posit<64, 3>;

#endif
