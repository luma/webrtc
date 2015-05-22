# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from profile_creators import profile_safe_url_list
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ProfileSafeUrlPage(page_module.Page):
  def __init__(self, url, page_set):
    super(ProfileSafeUrlPage, self).__init__(
        url=url,
        page_set = page_set,
        credentials_path = 'data/credentials.json')
    self.credentials = 'google'


class ProfileSafeUrlsPageSet(page_set_module.PageSet):
  """Safe urls used for profile generation."""

  def __init__(self):
    super(ProfileSafeUrlsPageSet, self).__init__(
      archive_data_file='data/profile_safe_urls.json',
      user_agent_type='desktop',
      bucket=page_set_module.PARTNER_BUCKET)

    # Only use the first 500 urls to prevent the .wpr files from getting too
    # big.
    safe_urls = profile_safe_url_list.GetShuffledSafeUrls()[0:500]
    for safe_url in safe_urls:
      self.AddUserStory(ProfileSafeUrlPage(safe_url, self))