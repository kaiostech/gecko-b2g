/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/prctl.h>
#include <signal.h>

#ifndef B2G_NAME
#  define B2G_NAME "b2g-bin"
#endif
#ifndef GAIA_PATH
#  define GAIA_PATH "gaia/profile"
#endif
#define NOMEM "Could not allocate enough memory"

#define API_DAEMON "api-daemon/api-daemon"
#define API_DAEMON_CONFIG "api-daemon/config.toml"
#define DEVICE_CAPABILITY_CONFIG "DEVICE_CAPABILITY_CONFIG=./api-daemon/devicecapability.json"
#define DEFAULT_SETTINGS "gaia/profile/settings.json"

void error(char* msg) { fprintf(stderr, "ERROR: %s\n", msg); }

int main(int argc, char* argv[], char* envp[]) {
  char* cwd = NULL;
  char* apidaemon_path = NULL;
  char* full_path = NULL;
  char* full_profile_path = NULL;
  char* full_config_path = NULL;
  int pid;
  // Add enternal environment path for desktop
  int length = 2;
  char* apidaemon_envp[length];

  printf("Starting %s\n", B2G_NAME);
  cwd = realpath(dirname(argv[0]), NULL);
  full_path = (char*)malloc(strlen(cwd) + strlen(B2G_NAME) + 2);
  apidaemon_path = (char*)malloc(strlen(cwd) + strlen(API_DAEMON) + 2);
  if (!full_path || !apidaemon_path) {
    free(full_path);
    free(apidaemon_path);
    free(cwd);
    error(NOMEM);
    return -2;
  }

  full_profile_path = (char*)malloc(strlen(cwd) + strlen(GAIA_PATH) + 2);
  full_config_path = (char*)malloc(strlen(cwd) + strlen(API_DAEMON_CONFIG) + 2);
  if (!full_profile_path || !full_config_path) {
    free(full_path);
    free(apidaemon_path);
    free(full_profile_path);
    free(full_config_path);
    free(cwd);
    error(NOMEM);
    return -2;
  }

  sprintf(apidaemon_path, "%s/%s", cwd, API_DAEMON);
  sprintf(full_config_path, "%s/%s", cwd, API_DAEMON_CONFIG);
  sprintf(full_path, "%s/%s", cwd, B2G_NAME);
  sprintf(full_profile_path, "%s/%s", cwd, GAIA_PATH);
  printf("Running: %s --profile %s\n", full_path, full_profile_path);
  printf("Running: %s %s\n", apidaemon_path, full_config_path);
  fflush(stdout);
  fflush(stderr);

  apidaemon_envp[0] = (char*)malloc(strlen(DEVICE_CAPABILITY_CONFIG) + 1);
  sprintf(apidaemon_envp[0], "%s", DEVICE_CAPABILITY_CONFIG);

  apidaemon_envp[1] = (char*)malloc(strlen(DEFAULT_SETTINGS) + strlen(cwd) + 1);
  sprintf(apidaemon_envp[1], "DEFAULT_SETTINGS=%s/%s",cwd, DEFAULT_SETTINGS);
  apidaemon_envp[2] = NULL;

  if ((pid = fork()) == 0) {
    // When b2g process is terminated, PR_SET_PDEATHSIG will be sent to kill the api-daemon
    // process at the same time.
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    // api-daemon loads vhost.root_path and apps_service.root_path from
    // config.toml by relative path, so the current working directory must be
    // set precisely.
    if (chdir(cwd)) {
      printf("chdir to %s for api-daemon error.\n", cwd);
    }
    execle(apidaemon_path, apidaemon_path, full_config_path, NULL, apidaemon_envp);
  } else if (pid < 0) {
    printf("fork api-daemon error\n");
  } else {
    // XXX: yes, the printf above says --profile and this execle uses -profile.
    // Bug 1088430 will change the execle to use --profile.
#ifndef ENABLE_TOUCH_SUPPORT
    execle(full_path, full_path, "-profile", full_profile_path, "-type", "bartype", NULL, envp);
#else
    execle(full_path, full_path, "-profile", full_profile_path, NULL, envp);
#endif
  }
  error("unable to start");
  perror(argv[0]);
  free(full_path);
  free(full_profile_path);
  free(apidaemon_path);
  free(full_config_path);
  free(cwd);
  return -1;
}
