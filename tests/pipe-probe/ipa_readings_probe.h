#pragma once
#include <vector>

// Runs against the real worker/pipes with --pipe-probe. No HWND, desktop input,
// system registration or visible candidate window is used.
static void ProbeIpaReadings(Pipe &main, Pipe &replies, Pipe &workers,
                             std::uint64_t client) {
  std::uint64_t request = 10000;
  std::wstring raw;
  std::vector<FanyImeNamedpipeDataToTsfWorkerThread> commits;
  auto drain = [&] {
    FanyImeNamedpipeDataToTsfWorkerThread update{};
    while (workers.TryRead(update))
      if (update.msg_type == FanyImeWorkerReplyType::CommitCurCandidate ||
          update.msg_type == FanyImeWorkerReplyType::CommitCandidateAndContinue)
        commits.push_back(update);
  };
  auto send = [&](UINT type, UINT key, wchar_t ch, UINT modifiers) {
    FanyImeNamedpipeData event{};
    event.event_type = type;
    event.client_id = client;
    event.request_id = ++request;
    event.keycode = key;
    event.wch = ch;
    event.modifiers_down = modifiers;
    event.pinyin_length = static_cast<int>(raw.size());
    std::copy(raw.begin(), raw.end(), event.pinyin_string);
    main.Write(event);
    return event.request_id;
  };
  auto key = [&](UINT vk, wchar_t ch = 0, UINT modifiers = 0) {
    const auto id = send(FanyImePipeEventType::KeyEvent, vk, ch, modifiers);
    FanyImeNamedpipeDataToTsf result{};
    const auto deadline = GetTickCount64() + 5000;
    bool received = false;
    while (GetTickCount64() < deadline) {
      drain();
      if (replies.TryRead(result)) {
        received = true;
        break;
      }
      Sleep(5);
    }
    Require(received, "Reading key reply timed out");
    Require(result.request_id == id, "Reading key reply lost its correlation");
    Sleep(
        20); // Navigation publishes shared memory immediately after its reply.
    drain();
    return result;
  };
  auto show = [&] { send(FanyImePipeEventType::ShowCandidateWnd, 0, 0, 0); };
  auto page = [&] {
    HANDLE mapping =
        OpenFileMappingW(FILE_MAP_READ, FALSE, FANY_IME_SHARED_MEMORY);
    Require(mapping != nullptr, "Candidate shared memory missing");
    auto data = static_cast<const FanyImeSharedMemoryData *>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Require(data != nullptr, "Candidate shared memory cannot be read");
    std::wstring result(data->candidate_string);
    UnmapViewOfFile(data);
    CloseHandle(mapping);
    return result;
  };
  auto type = [&](const std::wstring &pinyin) {
    send(FanyImePipeEventType::KeyEvent, VK_ESCAPE, 0,
         0); // locally consumed, no reply
    raw.clear();
    for (wchar_t ch : pinyin) {
      raw += ch;
      send(FanyImePipeEventType::KeyEvent, ch - L'a' + L'A', ch,
           0); // raw preedit: no letter reply
      show();
    }
    key(VK_DOWN);
    key(VK_UP); // ordered worker barrier, restores first highlight
  };
  auto highlight = [&](const std::wstring &word) {
    for (int attempts = 0; attempts < 40; ++attempts) {
      const auto text = page();
      size_t start = 0;
      int index = 0;
      while (start < text.size()) {
        const auto end = text.find(L',', start);
        const auto item = text.substr(start, end - start);
        if (item.rfind(word + L"<", 0) == 0 || item == word) {
          for (int i = 0; i < index; ++i)
            key(VK_DOWN);
          return index;
        }
        ++index;
        if (end == std::wstring::npos)
          break;
        start = end + 1;
      }
      key(VK_NEXT);
    }
    throw std::runtime_error(
        "Requested Hanzi missing from actual candidate pages");
  };

  auto numeric = [&](const std::wstring &word) {
    int index = highlight(word);
    return key(L'1' + index, L'1' + index);
  };
  auto exact = [&](const std::wstring &text) {
    drain();
    Require(commits.size() == 1 &&
                commits[0].msg_type ==
                    FanyImeWorkerReplyType::CommitCurCandidate &&
                std::wstring(commits[0].data) == text,
            "IPA did not reach the actual worker commit path exactly once");
    commits.clear();
    raw.clear();
  };

  type(L"yanwu");
  auto result = numeric(L"厌恶");
  Require(result.msg_type == FanyImeReplyType::OutofRange,
          "Default IPA selection leaked a normal Hanzi commit");
  exact(L"jan˥˩ wu˥˩");

  for (const auto &[digit, expected] :
       std::vector<std::pair<UINT, std::wstring>>{{L'1', L"xau˨˩˧"},
                                                  {L'2', L"xau˥˩"}}) {
    type(L"h");
    result = key(L'1', L'!', 1);
    Require(
        result.msg_type == FanyImeReplyType::OutofRange && commits.empty(),
        "Tone shortcut during spelling must preserve the pending composition");
    numeric(L"好");
    Require(commits.empty(),
            "First numeric selection committed before tone choice");
    Require(page().find(L"hao3") != std::wstring::npos &&
                page().find(L"hao4") != std::wstring::npos,
            "Single-letter candidate did not open the two readings of 好");
    key(digit, digit);
    exact(expected);
  }

  type(L"haoxue");
  numeric(L"好学");
  Require(commits.empty() && page().find(L"hao3 xue2") != std::wstring::npos &&
              page().find(L"hao4 xue2") != std::wstring::npos,
          "Whole-word ambiguity was guessed");
  key(L'2', L'2');
  exact(L"xau˥˩ ɕye˧˥");

  type(L"wu");
  numeric(L"恶");
  const auto choices = page();
  Require(commits.empty() && choices.find(L"wu1") != std::wstring::npos &&
              choices.find(L"wu4") != std::wstring::npos &&
              choices.find(L"e3") == std::wstring::npos &&
              choices.find(L"e4") == std::wstring::npos,
          "wu choice leaked e or committed prematurely");
  key(L'2', L'2');
  exact(L"wu˥˩");

  type(L"hangzhang");
  numeric(L"行");
  if (commits.empty()) {
    Require(page().find(L"hang") != std::wstring::npos,
            "Partial candidate did not expose its readings");
    key(L'1', L'1');
  }
  drain();
  Require(commits.size() == 1 &&
              commits[0].msg_type ==
                  FanyImeWorkerReplyType::CommitCandidateAndContinue &&
              std::wstring(commits[0].data).rfind(L"4\t", 0) == 0,
          "Partial IPA selection lost the exact consumed prefix length");
  Require(std::wstring(commits[0].data).find(L"行") == std::wstring::npos,
          "Partial selection committed Hanzi");
  commits.clear();
  raw = L"zhang";
  key(VK_DOWN);
  key(VK_UP);
  Require(page().find(L"长") != std::wstring::npos,
          "Unconsumed zhang lost its candidate page");
  send(FanyImePipeEventType::KeyEvent, VK_ESCAPE, 0, 0);
  raw.clear();

  for (UINT digit = L'1'; digit <= L'5'; ++digit) {
    result = key(digit, L'!', 1);
    const std::wstring expected(1,
                                static_cast<wchar_t>(0x02e9 - (digit - L'1')));
    Require(result.msg_type == FanyImeReplyType::CommitExactText &&
                std::wstring(result.candidate_string) == expected,
            "Shift+1-5 failed outside CIN mode");
  }
  Require(commits.empty(),
          "Tone shortcut also issued a duplicate worker commit");
  std::cout << "Real Server numeric IPA: complete word, h -> 好 -> hao3/hao4, "
               "whole-word ambiguity, "
               "wu/e filtering, partial composition and all five tone "
               "shortcuts passed\n";
}
