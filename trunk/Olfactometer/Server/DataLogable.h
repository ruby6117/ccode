#ifndef DataLogable_H
#define DataLogable_H

class DataLogable
{
protected:
  DataLogable() {}

public:
  virtual ~DataLogable() {}

  enum DataType { Cooked = 1,   Raw = 2,    Other = 4  };

  /// enables data logging for the specified datatype
  virtual bool setLoggingEnabled(bool enable, DataType t = Cooked) = 0;
  /// disable data logging for the specified datatype
  virtual bool loggingEnabled(DataType t = Cooked) const = 0;

protected:
  /// log a data point for the specified datatype -- a data point is a float -- if logging is disabled for that datatype nothing happens -- default implementation does nothing -- subclass in kernel side will do something
  virtual bool logDatum(double datum, DataType t = Cooked, const char *meta = 0) { (void)datum; (void)t; (void)meta; return false; }

private:
  DataLogable(const DataLogable &) {}  
  DataLogable & operator=(const DataLogable &) { return *this; }
};

#endif
