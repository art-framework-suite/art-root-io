#ifndef art_root_io_detail_RootErrorClassifier_h
#define art_root_io_detail_RootErrorClassifier_h

namespace art::detail {
  struct RootErrorPayload;

  class RootErrorClassifier {
  public:
    RootErrorClassifier(int level, RootErrorPayload const& payload);

    bool
    should_be_suppressed() const noexcept
    {
      return canSuppress_;
    }
    bool
    should_be_info_logged() const noexcept
    {
      return !canSuppress_ && canInfoLog_;
    }
    bool
    should_be_error_logged() const noexcept
    {
      return !canSuppress_ && !canInfoLog_ && canErrorLog_;
    }
    bool
    should_be_thrown() const noexcept
    {
      return !canSuppress_ && !canInfoLog_ && !canErrorLog_;
    }

  private:
    bool canSuppress_;
    bool canInfoLog_;
    bool canErrorLog_;
  };
}

#endif /* art_root_io_detail_RootErrorClassifier_h */

// Local Variables:
// mode: c++
// End:
