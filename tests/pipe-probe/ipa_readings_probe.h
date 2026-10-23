#pragma once
#include <vector>

// Runs against the real worker/pipes with --pipe-probe. No HWND, desktop input,
// system registration or visible candidate window is used.
static void ProbeIpaReadings(Pipe &main, Pipe &replies, Pipe &workers,
                             std::uint64_t client) {
  std::uint64_t request = 10000;
  std::wstring raw;
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
      FanyImeNamedpipeDataToTsfWorkerThread update{};
      while (workers.TryRead(update)) {
        Require(update.msg_type != FanyImeWorkerReplyType::CommitCurCandidate,
                "Unexpected asynchronous text commit");
      }
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
          return;
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

  type(L"yanwu");
  highlight(L"厌恶");
  auto result = key(VK_RETURN, L'\r', 2);
  Require(result.msg_type == FanyImeReplyType::CommitExactText &&
              std::wstring(result.candidate_string) == L"jan˥˩ wu˥˩",
          "Word-level reading did not commit yan4 wu4 IPA");

  type(L"haoxue");
  highlight(L"好学");
  Require(page().find(L"选择读音") != std::wstring::npos,
          "Ambiguous word has no choice prompt");
  result = key(VK_RETURN, L'\r', 2);
  Require(result.msg_type == FanyImeReplyType::NavigationIgnored,
          "Opening readings committed an arbitrary pronunciation");
  Require(page().find(L"hao3 xue2") != std::wstring::npos &&
              page().find(L"hao4 xue2") != std::wstring::npos,
          "Whole-word alternatives were lost");
  show();
  result = key(L'2', L'2');
  Require(result.msg_type == FanyImeReplyType::OutofRange,
          "Choosing a reading unexpectedly committed text");
  Require(page().find(L"xau˥˩ ɕye˧˥") != std::wstring::npos,
          "Chosen hao4 reading was not shown");
  result = key(VK_RETURN, L'\r', 3);
  Require(result.msg_type == FanyImeReplyType::NavigationIgnored,
          "Reopen reading shortcut failed");
  result =
      key(VK_RETURN, L'\r', 3); // cancel reopening, keep last confirmed choice
  Require(result.msg_type == FanyImeReplyType::NavigationIgnored,
          "Cancel reading shortcut failed");
  result = key(VK_RETURN, L'\r', 2);
  Require(result.msg_type == FanyImeReplyType::CommitExactText &&
              std::wstring(result.candidate_string) == L"xau˥˩ ɕye˧˥",
          "Confirmed reading was lost or another tone committed");

  type(L"wu");
  highlight(L"恶");
  result = key(VK_RETURN, L'\r', 2);
  Require(result.msg_type == FanyImeReplyType::NavigationIgnored,
          "wu ambiguity committed before choice");
  const auto choices = page();
  Require(choices.find(L"wu1") != std::wstring::npos &&
              choices.find(L"wu4") != std::wstring::npos &&
              choices.find(L"e3") == std::wstring::npos &&
              choices.find(L"e4") == std::wstring::npos,
          "wu alternatives are missing or contain an unrelated e reading");
  result = key(L'2', L'2');
  Require(result.msg_type == FanyImeReplyType::OutofRange,
          "Reading selection committed text");
  result = key(VK_SPACE, L' ');
  Require(result.msg_type == FanyImeReplyType::Normal &&
              std::wstring(result.candidate_string) == L"恶",
          "Hanzi selection was replaced by its IPA");
  std::cout << "Real Server reading flow: word tone, manual alternatives, no "
               "premature commit, reopen/cancel, "
               "wu/e filtering and separate Hanzi commit passed\n";
}
