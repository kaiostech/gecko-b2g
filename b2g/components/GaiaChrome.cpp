/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GaiaChrome.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsChromeRegistry.h"
#include "nsDirectoryServiceDefs.h"
#include "nsLocalFile.h"
#include "nsXULAppAPI.h"
#include "nsZipArchive.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Services.h"
#include "mozilla/FileLocation.h"
#include "mozilla/Omnijar.h"

#define NS_GAIACHROME_CID                            \
  {                                                  \
    0x83f8f999, 0x6b87, 0x4dd8, {                    \
      0xa0, 0x93, 0x72, 0x0b, 0xfb, 0x67, 0x4d, 0x38 \
    }                                                \
  }

using namespace mozilla;

StaticRefPtr<GaiaChrome> gGaiaChrome;

NS_IMPL_ISUPPORTS(GaiaChrome, nsIGaiaChrome)

GaiaChrome::GaiaChrome()
    : mPackageName("system"_ns),
      mAppsDir(u"webapps"_ns),
      mVrootDir(u"vroot"_ns),
      mDataRoot(u"/data/local"_ns),
      mSystemRoot(u"/system/b2g"_ns) {
  MOZ_ASSERT(NS_IsMainThread());

  GetProfileDir();
  Register();
}

// virtual
GaiaChrome::~GaiaChrome() {}

nsresult GaiaChrome::GetProfileDir() {
  nsCOMPtr<nsIFile> profDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profDir));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = profDir->Clone(getter_AddRefs(mProfDir));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult GaiaChrome::ComputeAppsPath(nsIFile* aPath) {
  nsCOMPtr<nsIFile> locationDetection = new nsLocalFile();
#if defined(MOZ_WIDGET_GONK)
  locationDetection->InitWithPath(mSystemRoot);
  locationDetection->Append(mAppsDir);
  bool appsInSystem = EnsureIsDirectory(locationDetection);
  locationDetection->InitWithPath(mDataRoot);
  locationDetection->Append(mAppsDir);
  bool appsInData = EnsureIsDirectory(locationDetection);

  if (!appsInData && !appsInSystem) {
    printf_stderr("!!! No base directory with apps found.");
    // Just return an error instead of crashing, since it will be obvious
    // anyway if resources don't load as expected.
    return NS_ERROR_UNEXPECTED;
  }

  aPath->InitWithPath(appsInData ? mDataRoot : mSystemRoot);
#else
  // On non-device builds, use the profile directory.
  aPath->InitWithFile(mProfDir);
#endif

  aPath->Append(mAppsDir);
  aPath->Append(u"."_ns);

#if defined(MOZ_WIDGET_GONK)
  if (appsInData) {
    locationDetection->Append(mVrootDir);
    if (EnsureIsDirectory(locationDetection)) {
      aPath->Append(mVrootDir);
      aPath->Append(u"."_ns);
    }
  }
#else

  locationDetection->InitWithFile(aPath);
  locationDetection->Append(mVrootDir);
  if (EnsureIsDirectory(locationDetection)) {
    aPath->Append(mVrootDir);
    aPath->Append(u"."_ns);
  }
#endif

  return NS_OK;
}

bool GaiaChrome::EnsureIsDirectory(nsIFile* aPath) {
  bool isDir = false;
  aPath->IsDirectory(&isDir);
  return isDir;
}

nsresult GaiaChrome::EnsureValidSystemPath(nsIFile* appsDir) {
  // Ensure there is a valid "apps/system" directory
  nsCOMPtr<nsIFile> systemAppDir = new nsLocalFile();
  systemAppDir->InitWithFile(appsDir);
  systemAppDir->Append(u"system"_ns);

  bool hasSystemAppDir = EnsureIsDirectory(systemAppDir);
  if (!hasSystemAppDir) {
    nsCString path;
    appsDir->GetNativePath(path);
    // We don't want to continue if the apps path does not exists ...
    printf_stderr("!!! Gaia chrome package is not a directory: %s\n",
                  path.get());
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
GaiaChrome::Register() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsChromeRegistry::gChromeRegistry != nullptr);

  nsCOMPtr<nsIFile> aPath = new nsLocalFile();
  nsresult rv = ComputeAppsPath(aPath);
  NS_ENSURE_SUCCESS(rv, rv);

  FileLocation appsLocation;

  // Check if there is a $APPS_DIR/system/application.zip file present.
  // If so, we'll use it as the content of the system app
  // served under chrome://system/content/
  nsCOMPtr<nsIFile> zipFile;
  // Use a clone to not modify aPath since we need it as is later
  // when there is no zip file.
  rv = aPath->Clone(getter_AddRefs(zipFile));
  NS_ENSURE_SUCCESS(rv, rv);
  zipFile->Append(NS_ConvertUTF8toUTF16(mPackageName));
  zipFile->Append(NS_ConvertUTF8toUTF16("application.zip"));
  bool zipExists = false;
  zipFile->Exists(&zipExists);

  if (zipExists) {
    appsLocation = FileLocation(zipFile, "/");
  } else {
    // Check that we have a directory for the system app only
    // when loading from a file hierarchy, but not from a zip.
    nsresult rv = EnsureValidSystemPath(aPath);
    NS_ENSURE_SUCCESS(rv, rv);
    aPath->Append(u"system"_ns);
    aPath->Append(u"."_ns);
    appsLocation = FileLocation(aPath);
  }

  nsCString uri;
  appsLocation.GetURIString(uri);

  char* argv[2];
  argv[0] = (char*)mPackageName.get();
  argv[1] = (char*)uri.get();

  nsChromeRegistry::ManifestProcessingContext cx(NS_APP_LOCATION, appsLocation);
  nsChromeRegistry::gChromeRegistry->ManifestContent(cx, 0, argv, 0);

  return NS_OK;
}

already_AddRefed<GaiaChrome> GaiaChrome::FactoryCreate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gGaiaChrome) {
    gGaiaChrome = new GaiaChrome();
    ClearOnShutdown(&gGaiaChrome);
  }

  RefPtr<GaiaChrome> service = gGaiaChrome.get();
  return service.forget();
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(GaiaChrome, GaiaChrome::FactoryCreate)

NS_DEFINE_NAMED_CID(NS_GAIACHROME_CID);

static const mozilla::Module::CIDEntry kGaiaChromeCIDs[] = {
    {&kNS_GAIACHROME_CID, false, nullptr, GaiaChromeConstructor}, {nullptr}};

static const mozilla::Module::ContractIDEntry kGaiaChromeContracts[] = {
    {"@mozilla.org/b2g/gaia-chrome;1", &kNS_GAIACHROME_CID}, {nullptr}};

extern const mozilla::Module kGaiaChromeModule = {
    mozilla::Module::kVersion, kGaiaChromeCIDs, kGaiaChromeContracts, nullptr};
