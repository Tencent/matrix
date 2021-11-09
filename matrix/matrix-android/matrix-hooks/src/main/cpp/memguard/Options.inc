//
// Created by tomystang on 2020/10/18.
//

OPTION_ITEM(size_t, maxAllocationSize, 64 * 1024,
      "Max allocation size MemGuard can detect its under/overflow "
      "issues.")

OPTION_ITEM(size_t, maxDetectableAllocationCount, 16384,
      "Max allocation count MemGuard can detect its under/overflow "
      "issues.")

OPTION_ITEM(size_t, maxSkippedAllocationCount, 5,
      "Max skipped allocation count between two guarded allocations. "
      "For example, if 5 was set to this option, MemGuard will generate a "
      "random number 'k' in range [0,5] and the first k-th allocations "
      "will be ignored.")

OPTION_ITEM(size_t, percentageOfLeftSideGuard, 30,
      "Probability of putting guard page on the left side of specific pointer. "
      "For example, if 30 was set to this option, the probability of a pointer being "
      "guarded on the left side will be 30%, and the probability of a pointer being guarded "
      "on the right side will be 70%.")

OPTION_ITEM(bool, perfectRightSideGuard, false,
      "Whether MemGuard should return a pointer with guard page on right side without "
      "gaps. If true was set to this option, overflow issue will be easier to be detected "
      "but the returned pointer may not be aligned properly. Sometimes these not aligned "
      "pointers can crash your app.")

OPTION_ITEM(bool, ignoreOverlappedReading, false,
      "Whether MemGuard should regard overlapped reading as an issue.")

OPTION_ITEM(std::string, issueDumpFilePath, "",
      "Path to write dump file when memory issue was detected. Leave it empty "
      "will make MemGuard not dump issue info into file.")

OPTION_ITEM(std::vector<std::string>, targetSOPatterns, std::vector<std::string>(),
      "Patterns described by RegEx of target libs that we want to detect any memory issues.")

OPTION_ITEM(std::vector<std::string>, ignoredSOPatterns, std::vector<std::string>(),
      "Patterns described by RegEx of target libs that we want to skip for detecting any memory issues.")
