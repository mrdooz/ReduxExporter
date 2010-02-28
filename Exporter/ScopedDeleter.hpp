#ifndef SCOPED_DELETER_HPP
#define SCOPED_DELETER_HPP

/**
 * Scoped deleter is an anonymous object that is connected to a delete function which it
 * calls when it goes out of scope.
 * See http://www.dooz.se/blog/?p=202#content for more details.
 */

struct Command {
  virtual void Execute() = 0;
};

template< class T, typename U >
struct CommandT : public Command {
  CommandT(T fn, U arg)
    : fn_(fn)
    , arg_(arg) {
  }

  virtual void Execute() {
    fn_(arg_);
  }

  T fn_;
  U arg_;
};

template<class T>
struct CommandC : public Command {
  typedef void(T::*NullaryPfn)();
  CommandC(T obj, NullaryPfn fn)
    : obj_(obj)
    , fn_(fn) {
  }

  virtual void Execute() {
    (obj_.*fn_)();
  }

  T obj_;
  NullaryPfn fn_;
};

template< class T>
Command* Maker(void(T::*fn)(), T obj ) {
  return new CommandC<T>(obj, fn);
}

template< class T, class U >
Command* Maker(T cmd, U arg) {
  return new CommandT<T, U>(cmd, arg);
}

template<class T>
void Deleter(T* t) {
  delete t;
}

template<class T>
void ArrayDeleter(T* t) {
  delete [] t;
}

template<class T>  
void FileCloser(T t) {
  fclose(t);
} 

struct ScopedDeleter {
  ScopedDeleter(Command* cmd)
    : cmd_(cmd) {
  }

  ~ScopedDeleter() {
    cmd_->Execute();
    delete cmd_;
  }
  Command* cmd_;
};

#define GEN_NAME2(prefix, line) prefix##line
#define GEN_NAME(prefix, line) GEN_NAME2(prefix, line)
#define SCOPED_DELETER(fn, obj) ScopedDeleter GEN_NAME(deleter, __LINE__)(Maker(fn, obj))

#endif
