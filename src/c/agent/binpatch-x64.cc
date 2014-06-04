//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

class X64 : public InstructionSet {
public:
  virtual size_t get_redirect_size_bytes();
  virtual void install_redirect(PatchRequest &request);

  // Returns the singleton ia32 instance.
  static X64 &get();

private:
  static const byte_t kJmpOp = 0xE9;
  static const size_t kJmpOpSizeBytes = 5;
  static const byte_t kInt3Op = 0xCC;
  static const byte_t kPushaOp = 0x60;
  static const byte_t kFarCallOp = 0xE8;
  static const size_t kFarCallOpSizeBytes = 5;

  static const size_t kRedirectSizeBytes = kJmpOpSizeBytes;
};

size_t X64::get_redirect_size_bytes() {
  return kRedirectSizeBytes;
}

void X64::install_redirect(PatchRequest &request) {
  address_t original = request.original();
  address_t replacement = request.replacement();

  // Write the redirect to the entry stub into the original.
  ssize_t original_to_replacement = (replacement - original) - kJmpOpSizeBytes;
  original[0] = kJmpOp;
  reinterpret_cast<int32_t*>(original + 1)[0] = static_cast<int32_t>(original_to_replacement);
}

X64 &X64::get() {
  static X64 *instance = NULL;
  if (instance == NULL)
    instance = new X64();
  return *instance;
}
