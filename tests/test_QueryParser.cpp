#include <gtest/gtest.h>

#include "QueryParser.h"

using Keys = std::vector<std::string>;

TEST(QueryParser, ParsesKeyValuePairs)
{
  QueryParser q("a=1&b=two");
  EXPECT_EQ(q.get("a"), "1");
  EXPECT_EQ(q.get("b"), "two");
  EXPECT_EQ(q.keys(), (Keys{ "a", "b" }));
}

TEST(QueryParser, DecodesPercentEscapesAndPlus)
{
  QueryParser q("device=%2Fdev%2Fhidg0&name=a+b");
  EXPECT_EQ(q.get("device"), "/dev/hidg0");
  EXPECT_EQ(q.get("name"), "a b");
}

TEST(QueryParser, MissingKeyReturnsEmptyOrDefault)
{
  QueryParser q("a=1");
  EXPECT_EQ(q.get("x"), "");
  EXPECT_EQ(q.get("x", "fallback"), "fallback");
  EXPECT_EQ(q.get("a", "fallback"), "1");
}

TEST(QueryParser, EmptyBodyHasNoKeys)
{
  QueryParser q("");
  EXPECT_TRUE(q.keys().empty());
}

TEST(QueryParser, EmptyValueIsKept)
{
  QueryParser q("a=&b=2");
  EXPECT_EQ(q.keys(), (Keys{ "a", "b" }));
  EXPECT_EQ(q.get("a", "fallback"), "");
}

TEST(QueryParser, OnlyFirstEqualsSignSplits)
{
  QueryParser q("a=b=c");
  EXPECT_EQ(q.get("a"), "b=c");
}

TEST(QueryParser, LaterDuplicateKeyWins)
{
  QueryParser q("a=1&a=2");
  EXPECT_EQ(q.get("a"), "2");
}

TEST(QueryParser, TruncatedPercentEscapeStopsDecoding)
{
  QueryParser q("a=50%2");
  EXPECT_EQ(q.get("a"), "50");
}

// A field without "=" (e.g. "flag") must not crash the parser.
TEST(QueryParser, PairWithoutEqualsSignDoesNotThrow)
{
  EXPECT_NO_THROW({
    QueryParser q("a=1&flag&c=3");
    EXPECT_EQ(q.get("a"), "1");
  });
}

// The body the /config page posts. An unchecked checkbox is not sent at all.
TEST(QueryParser, ConfigFormBody)
{
  QueryParser q("device=%2Fdev%2Fhidg0&ip=0.0.0.0&port=18080&cam=0&cam_width=640"
                "&cam_height=480&cam_framerate=80&action=Save");
  EXPECT_EQ(q.get("device"), "/dev/hidg0");
  EXPECT_EQ(q.get("ip"), "0.0.0.0");
  EXPECT_EQ(q.get("port"), "18080");
  EXPECT_EQ(q.get("cam_framerate"), "80");
  EXPECT_EQ(q.get("action"), "Save");
  EXPECT_EQ(q.get("cam_verbose"), "");

  QueryParser checked("cam_verbose=cam_verbose&action=Set");
  EXPECT_EQ(checked.get("cam_verbose"), "cam_verbose");
}
