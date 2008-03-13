#ifndef RTFifo_H
#define RTFifo_H


/** @brief A Class encapsulating realtime-fifos.

    If compiled for userspace, reads/writes to /dev/rtfN.  If compiled
    for kernelspace, it assumes you are using it in a realtime program
    so it uses rtf_get and rtf_put. */
class RTFifo
{
public:  
  RTFifo();
  /** Constructor that:
      1. calls open(size, minor) for you 
      2. calls enableHandler(useHandler) for you
      3. calls enableRTHandler(useRTHandler) for you */
  RTFifo(int size, int minor = -1, bool useHandler = false, bool useRTHandler = false);

  /// Calls close, does cleanup
  virtual ~RTFifo();

  /// Opens the fifo 'minor'.  If minor is negative, probe for a free minor.
  bool open(int size, int minor);

#ifndef __KERNEL__
  /// in userspace, the size is ignored
  bool open(int minor) { return open(0, minor); }
#endif
  
  /// Closes the fifo.  Note about user pairs: If in kernel space, also closes the buddy fifo if we have a user pair buddy (see makeUserPair() )
  void close();

  /** Reads at most nbytes from the fifo puts them in the memory pointed to by
      buf.  
      @returns the number of bytes actually read or negative on error. */
  int read(void *buf, unsigned nbytes);
  /** Writes at most nbytes to the fifo puts from the memory pointed to by buf.
      @returns the number of bytes actually written or negative on error. */
  int write(const void *buf, unsigned nbytes);

  /// @returns the minor/descriptor of the fifo or negative if it isn't opened
  int descr() const { return minor; }
  
  /// @returns the size of the fifo or negative if it isn't opened
  int size() const { return sz_bytes; }

  /// just in case you want this class instance to be synonymous with its descriptor
  operator int() const { return descr(); }

  /** @brief Enable/disable calling of handler() on fifo read/write.

      Only used in kernel: If this is called with true, then the handler() 
      virtual function is invoked (in non-realtime kernel space context) 
      whenever userspace writes to or reads from the fifo.  
      Default is disabled.*/
  void enableHandler(bool = true);
  /** @brief Enable/disable calling of handlerRT() on fifo read/write.

      Only used in kernel: If this is called with true, then the handlerRT() 
      virtual function is invoked (in realtime kernel space context) 
      whenever kernel space writes to the fifo via rtf_put() or reads from
      the fifo via rtf_get(). Defaults to disabled. */
  void enableRTHandler(bool = true); 

  /// Convenience method which is the same as calling enableHandler(false)
  void disableHandler() { enableHandler(false); }
  /// Convenience method which is the same as calling enableRTHandler(false)
  void disableRTHandler() { enableRTHandler(false); }
  
  /// Just like rtf_make_user_pair() in rtlinux documentation.  Used in kernel space only!
  static bool makeUserPair(RTFifo *get_fifo, RTFifo *put_fifo);
  /// @returns the buddy from a previous call to makeUserPair()
  RTFifo *userPairBuddy() const { return buddy; }

  /** @brief Tells you the number of bytes available for reading on the fifo.
      @returns the number of bytes available for reading from the fifo (like the ioctl of the same name) */
  unsigned int fionread() const;

  /** @brief Wait for more data on a fifo, with an optional timeout
      Note that this function does not behave correctly in realtime/kernel-- it
      always returns right away!
      @param num_usecs is the number of microseconds to wait for more data
             a 0 value means return immediately and a negative value means
             wait forever.  
      @returns true if data is available (reading will not block) */      
  bool waitData(int num_usecs) const;

protected:

  /// reimplement in subclasses to have a handler for the fifo (kernel space handlers run in non-realtime context whenever a user process reads/writes to the fifo -- userspace has no handler mechanism)
  virtual int handler() { return 0; }
  
  /// reimplement in subclasses to have an rt-handler for the fifo (kernel space handlers run in realtime context whenever another realtime process reads/writes to the fifo -- userspace has no handler mechanism)
  virtual int handlerRT() { return 0; }

  static RTFifo *handlerMap[];
  static int handlerWrapper(unsigned fifo);
  static int handlerWrapper_RT(unsigned fifo);

private:
  int minor, sz_bytes;
  RTFifo *buddy;
};

#endif
