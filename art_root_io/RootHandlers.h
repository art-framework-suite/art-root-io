#ifndef art_root_io_RootHandlers_h
#define art_root_io_RootHandlers_h

namespace art {

  class RootHandlers {
  public:
    RootHandlers();
    virtual ~RootHandlers();
    void disableErrorHandler();
    void enableErrorHandler();

  private:
    virtual void disableErrorHandler_() = 0;
    virtual void enableErrorHandler_() = 0;
  }; // RootHandlers

} // namespace art

#endif /* art_root_io_RootHandlers_h */

// Local Variables:
// mode: c++
// End:
