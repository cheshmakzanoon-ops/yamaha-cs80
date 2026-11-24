#include "tests/InstrumentBodyHostTests.h"
#include "tests/TypedValueUnitTests.h"
int main()
{
    const auto check = [](bool, const auto&) {};
    cs80::tests::testInstrumentBodyHost(check);
    cs80::tests::testTypedValueUnits(check);
}
