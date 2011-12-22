#define BOOST_TEST_MODULE unittest
#include <boost/test/included/unit_test.hpp>

// Always use only one test suite, otherwise the device will become blocked.
#include "v4l2_test.h"
//#include "gst_test.h"
//#include "usb_test.h"

// You can use the following setups: 
// BOOST_CHECK_MESSAGE(1 == 1, "Send test sucessfully");
// BOOST_CHECK( 1 == 1);
// BOOST_CHECK_EQUAL(1, 1);
// BOOST_WARN( 1 > 2);
// BOOST_REQUIRE_EQUAL(1,2);
// BOOST_FAIL("Should never reach the line");
// BOOST_ERROR("Some error");
// BOOST_TEST_CHECKPOINT("Checkpoint message");
// BOOST_MESSAGE
// BOOST_TEST_MESSAGE
// BOOST_REQUIRE_NO_THROW
// BOOST_REQUIRE_THROW
