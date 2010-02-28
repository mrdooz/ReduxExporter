#ifndef REDUX_EXPORTER_STUB
#define REDUX_EXPORTER_STUB

class ReduxExporterStub : public MPxFileTranslator
{
public:

	ReduxExporterStub(void);
	~ReduxExporterStub(void);

	MStatus	reader( const MFileObject& file, const MString& optionsString, FileAccessMode mode);
	MStatus	writer( const MFileObject& file, const MString& optionsString, FileAccessMode mode );

  bool haveReadMethod() const { return false; }
  bool haveWriteMethod() const { return true; }
  MString defaultExtension () const;

	MFileKind identifyFile(const MFileObject& fileName, const char* buffer, short size) const;
	static void* creator();
private:
  void    parse_options(const MString& options);
};

#endif // #ifndef REDUX_EXPORTER_STUB
