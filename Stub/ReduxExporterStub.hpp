#ifndef REDUX_EXPORTER_STUB
#define REDUX_EXPORTER_STUB

class ReduxExporterStub : public MPxFileTranslator
{
public:

	ReduxExporterStub(void);
	~ReduxExporterStub(void);

	virtual MStatus	reader( const MFileObject& file, const MString& optionsString, FileAccessMode mode);
	virtual MStatus	writer( const MFileObject& file, const MString& optionsString, FileAccessMode mode );

  virtual bool haveReadMethod() const { return false; }
  virtual bool haveWriteMethod() const { return true; }
  virtual MString defaultExtension () const;

	MFileKind identifyFile(const MFileObject& fileName, const char* buffer, short size) const;
	static void* creator();
private:
  void    parse_options(const MString& options);
};

#endif // #ifndef REDUX_EXPORTER_STUB
