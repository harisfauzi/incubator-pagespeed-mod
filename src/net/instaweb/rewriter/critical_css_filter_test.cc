/*
 * Copyright 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: slamm@google.com (Stephen Lamm)

#include "net/instaweb/rewriter/public/critical_css_filter.h"

#include "net/instaweb/rewriter/critical_css.pb.h"
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kRequestUrl[] = "http://test.com";

}  // namespace

namespace net_instaweb {

class Statistics;

namespace {

class MockCriticalCssFinder : public CriticalCssFinder {
 public:
  explicit MockCriticalCssFinder(RewriteDriver* driver, Statistics* stats)
      : CriticalCssFinder(stats),
        driver_(driver) {
  }

  void AddCriticalCss(const StringPiece& url, const StringPiece& rules,
                      int original_size) {
    if (critical_css_result_.get() == NULL) {
      critical_css_result_.reset(new CriticalCssResult());
    }
    CriticalCssResult_LinkRules* link_rules =
        critical_css_result_->add_link_rules();
    link_rules->set_link_url(url.as_string());
    link_rules->set_critical_rules(rules.as_string());
    link_rules->set_original_size(original_size);
  }

  void SetCriticalCssStats(
      int exception_count, int import_count, int link_count) {
    if (critical_css_result_.get() == NULL) {
      critical_css_result_.reset(new CriticalCssResult());
    }
    critical_css_result_->set_exception_count(exception_count);
    critical_css_result_->set_import_count(import_count);
    critical_css_result_->set_link_count(link_count);
  }

  // Mock to avoid dealing with property cache.
  CriticalCssResult* GetCriticalCssFromCache(RewriteDriver* driver) {
    return critical_css_result_.release();
  }

  void ComputeCriticalCss(StringPiece url, RewriteDriver* driver) {}
  const char* GetCohort() const { return "critical_css"; }

 private:
  RewriteDriver* driver_;
  scoped_ptr<CriticalCssResult> critical_css_result_;
};

}  // namespace

class CriticalCssFilterTest : public RewriteTestBase {
 public:
  CriticalCssFilterTest() {  }

  virtual bool AddHtmlTags() const { return false; }

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();

    finder_ = new MockCriticalCssFinder(rewrite_driver(), statistics());
    server_context()->set_critical_css_finder(finder_);

    filter_.reset(new CriticalCssFilter(rewrite_driver(), finder_));
    rewrite_driver()->AddFilter(filter_.get());

    ResetDriver();

    options_->DisableFilter(RewriteOptions::kDebug);
  }

  void ResetDriver() {
    PropertyCache* pcache = page_property_cache();
    server_context_->set_enable_property_cache(true);
    SetupCohort(pcache, finder_->GetCohort());

    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    pcache->set_enabled(true);
    pcache->Read(page);
  }

  scoped_ptr<CriticalCssFilter> filter_;
  MockCriticalCssFinder* finder_;  // owned by server_context
};

TEST_F(CriticalCssFilterTest, UnchangedWhenPcacheEmpty) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>a {color: red }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  ValidateExpected("unchanged_when_pcache_empty", input_html, input_html);

  // TODO(slamm): webkit headless gets called
}

TEST_F(CriticalCssFilterTest, InlineAndMove) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <style media=\"print\">a_used {color: azure }</style>"
      "<style>b_used {color: blue }</style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script type=\"text/javascript\">//<![CDATA[\n",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 64,"
      "  'total_original_external_size': 6,"
      "  'total_overhead_size': 85,"
      "  'num_replaced_links': 3,"
      "  'num_unreplaced_links': 0"
      "};"
      "\n//]]></script>");

  finder_->AddCriticalCss("http://test.com/a.css", "a_used {color: azure }", 1);
  finder_->AddCriticalCss("http://test.com/b.css", "b_used {color: blue }", 2);
  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }", 3);

  ValidateExpected("inline_and_move", input_html, expected_html);
}

TEST_F(CriticalCssFilterTest, InvalidUrl) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='Hi there!' type='text/css'>"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='Hi there!' type='text/css'>"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='Hi there!' type='text/css'>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script type=\"text/javascript\">//<![CDATA[\n",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 21,"
      "  'total_original_external_size': 33,"
      "  'total_overhead_size': 21,"
      "  'num_replaced_links': 1,"
      "  'num_unreplaced_links': 1"
      "};"
      "\n//]]></script>");

  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }", 33);

  ValidateExpected("invalid_url", input_html, expected_html);
}

TEST_F(CriticalCssFilterTest, NullAndEmptyCriticalRules) {
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<style></style>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color: cyan }</style>\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script type=\"text/javascript\">//<![CDATA[\n",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 21,"
      "  'total_original_external_size': 10,"
      "  'total_overhead_size': 42,"
      "  'num_replaced_links': 2,"
      "  'num_unreplaced_links': 1"
      "};"
      "\n//]]></script>");

  // Skip adding a critical CSS for a.css.
  //     In the filtered html, the original link is left in place and
  //     a duplicate link is added to the full set of CSS at the bottom
  //     to make sure CSS rules are applied in the correct order.

  finder_->AddCriticalCss("http://test.com/b.css", "", 4);  // no critical rules
  finder_->AddCriticalCss("http://test.com/c.css", "c_used {color: cyan }", 6);

  ValidateExpected("null_and_empty_critical_rules", input_html, expected_html);
}

TEST_F(CriticalCssFilterTest, DebugFilterAddsStats) {
  options_->ForceEnableFilter(RewriteOptions::kDebug);
  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  GoogleString expected_html = StrCat(
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<style></style>"
      "<!--Critical CSS applied:\n"
      "critical_size=0\n"
      "original_size=222\n"
      "original_src=http://test.com/b.css\n"
      "-->\n"
      "  <style type='text/css'>t {color: turquoise }</style>\n"
      "  World!\n"
      "  <style>c_used {color:cyan}</style>"
      "<!--Critical CSS applied:\n"
      "critical_size=19\n"
      "original_size=333\n"
      "original_src=http://test.com/c.css\n"
      "-->\n"
      "</body>\n"
      "<noscript id=\"psa_add_styles\">"
      "<link rel='stylesheet' href='a.css' type='text/css' media='print'>"
      "<link rel='stylesheet' href='b.css' type='text/css'>"
      "<style type='text/css'>t {color: turquoise }</style>"
      "<link rel='stylesheet' href='c.css' type='text/css'>"
      "</noscript>"
      "<script type=\"text/javascript\">//<![CDATA[\n",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 19,"
      "  'total_original_external_size': 555,"
      "  'total_overhead_size': 40,"
      "  'num_replaced_links': 2,"
      "  'num_unreplaced_links': 1"
      "};"
      "\n//]]></script>"
      "<!--Additional Critical CSS stats:\n"
      "  num_repeated_style_blocks=1\n"
      "  repeated_style_blocks_size=21\n"
      "\n"
      "From computing the critical CSS:\n"
      "  unhandled_import_count=8\n"
      "  unhandled_link_count=11\n"
      "  exception_count=5\n"
      "-->");

  // Skip adding a critical CSS for a.css.
  //     In the filtered html, the original link is left in place and
  //     a duplicate link is added to the full set of CSS at the bottom
  //     to make sure CSS rules are applied in the correct order.

  finder_->AddCriticalCss("http://test.com/b.css",
                          "", 222);  // no critical rules
  finder_->AddCriticalCss("http://test.com/c.css",
                          "c_used {color:cyan}", 333);
  finder_->SetCriticalCssStats(5, 8, 11);

  ValidateExpected("stats_flag_adds_stats", input_html, expected_html);
}

}  // namespace net_instaweb
