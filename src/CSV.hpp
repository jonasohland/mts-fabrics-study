#pragma once

#include <csv2/parameters.hpp>
#include <csv2/reader.hpp>
#include <csv2/writer.hpp>

namespace riedel::fabricsperf::csv
{
    using Reader = csv2::Reader<csv2::delimiter<','>, csv2::quote_character<'"'>,
        csv2::first_row_is_header<false>, csv2::trim_policy::trim_whitespace>;
    using Writer = csv2::Writer<csv2::delimiter<','>>;
}
