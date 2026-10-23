#include "tests/includes/test_framework.h"
#include "msimeui/Controls.h"
#include <cmath>

TEST_CASE(candidate_field_fonts_use_their_own_metrics_and_cache_keys)
{
    msimeui::CandidateList list;
    msimeui::CandidateList::Appearance appearance;
    appearance.fontFamily = L"Segoe UI";
    appearance.fontSize = 24;
    appearance.fitContent = true;
    list.SetAppearance(appearance);
    msimeui::CandidateList::Item item;
    item.text = L"WWWWWWWWWWWW";
    item.textFontFamily = L"Arial";
    list.SetItems({item});
    const auto primaryArial = list.Measure({2000, 2000}).width;
    item.textFontFamily = L"Courier New";
    list.SetItems({item});
    REQUIRE(std::abs(list.Measure({2000, 2000}).width - primaryArial) > 10);
    item.textFontFamily = L"Arial";
    list.SetItems({item});
    REQUIRE(list.Measure({2000, 2000}).width == primaryArial);

    item.translation = item.text;
    item.text = L"汉字";
    item.textFontFamily.clear();
    item.secondaryIsIpa = true;
    item.secondaryFontFamily = L"Arial";
    list.SetItems({item});
    const auto secondaryArial = list.Measure({2000, 2000}).width;
    item.secondaryFontFamily = L"Courier New";
    list.SetItems({item});
    REQUIRE(std::abs(list.Measure({2000, 2000}).width - secondaryArial) > 10);
    item.secondaryFontFamily = L"Arial";
    list.SetItems({item});
    REQUIRE(list.Measure({2000, 2000}).width == secondaryArial);
}
