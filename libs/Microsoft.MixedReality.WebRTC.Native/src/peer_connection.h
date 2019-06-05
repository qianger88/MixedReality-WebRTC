// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include "base/memory/ref_counted.h"

#include "audio_frame_observer.h"
#include "callback.h"
#include "data_channel_observer.h"
#include "video_frame_observer.h"

namespace mrtk {
namespace net {
namespace webrtc_impl {

class PeerConnection;

class PeerConnection : public webrtc::PeerConnectionObserver,
                       public webrtc::CreateSessionDescriptionObserver {
 public:
  PeerConnection();
  ~PeerConnection() noexcept override;
  void setPeerImpl(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer) noexcept;

  using ConnectedCallback = Callback<>;
  inline void registerConnectedCallback(ConnectedCallback&& callback) noexcept {
    auto lock = std::lock_guard{connected_callback_mutex_};
    connected_callback_ = std::move(callback);
  }

  using LocalSdpReadytoSendCallback = Callback<const char*, const char*>;
  inline void registerLocalSdpReadytoSendCallback(
      LocalSdpReadytoSendCallback&& callback) noexcept {
    auto lock = std::lock_guard{local_sdp_ready_to_send_callback_mutex_};
    local_sdp_ready_to_send_callback_ = std::move(callback);
  }

  using IceCandidateReadytoSendCallback =
      Callback<const char*, int, const char*>;
  inline void registerIceCandidateReadytoSendCallback(
      IceCandidateReadytoSendCallback&& callback) noexcept {
    auto lock = std::lock_guard{ice_candidate_ready_to_send_callback_mutex_};
    ice_candidate_ready_to_send_callback_ = std::move(callback);
  }

  using RenegotiationNeededCallback = Callback<>;
  inline void registerRenegotiationNeededCallback(
      RenegotiationNeededCallback&& callback) noexcept {
    auto lock = std::lock_guard{renegotiation_needed_callback_mutex_};
    renegotiation_needed_callback_ = std::move(callback);
  }

  inline void registerLocalVideoFrameCallback(
      I420FrameReadyCallback callback) noexcept {
    if (local_video_observer_) {
      local_video_observer_->setCallback(std::move(callback));
    }
  }

  inline void registerLocalVideoFrameCallback(
      ARGBFrameReadyCallback callback) noexcept {
    if (local_video_observer_) {
      local_video_observer_->setCallback(std::move(callback));
    }
  }

  inline void registerRemoteVideoFrameCallback(
      I420FrameReadyCallback callback) noexcept {
    if (remote_video_observer_) {
      remote_video_observer_->setCallback(std::move(callback));
    }
  }

  inline void registerRemoteVideoFrameCallback(
      ARGBFrameReadyCallback callback) noexcept {
    if (remote_video_observer_) {
      remote_video_observer_->setCallback(std::move(callback));
    }
  }

  bool addLocalVideoTrack(
      rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track) noexcept;
  void removeLocalVideoTrack() noexcept;
  bool addLocalAudioTrack(
      rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track) noexcept;
  void removeLocalAudioTrack() noexcept;
  bool addDataChannel(int id,
                      const char* label,
                      bool ordered,
                      bool reliable,
                      DataChannelMessageCallback message_callback,
                      DataChannelBufferingCallback buffering_callback,
                      DataChannelStateCallback state_callback) noexcept;
  bool removeDataChannel(int id) noexcept;
  bool removeDataChannel(const char* label) noexcept;

  bool sendDataChannelMessage(int id, const void* data, uint64_t size) noexcept;

  bool addIceCandidate(const char* sdp_mid,
                       const int sdp_mline_index,
                       const char* candidate) noexcept;
  bool createOffer() noexcept;
  bool createAnswer() noexcept;
  bool setRemoteDescription(const char* type, const char* sdp) noexcept;

 protected:
  // PeerConnectionObserver interface

  // Triggered when the SignalingState changed.
  void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState
                             new_state) noexcept override;

  // Triggered when media is received on a new stream from remote peer.
  void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>
                       stream) noexcept override;

  // Triggered when a remote peer closes a stream.
  void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface>
                          stream) noexcept override;

  // Triggered when a remote peer opens a data channel.
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
#if defined(WINUWP)
      noexcept(false)
#else
      noexcept
#endif
          override;

  // Triggered when renegotiation is needed. For example, an ICE restart
  // has begun.
  void OnRenegotiationNeeded() noexcept override;

  // Called any time the IceConnectionState changes.
  //
  // Note that our ICE states lag behind the standard slightly. The most
  // notable differences include the fact that "failed" occurs after 15
  // seconds, not 30, and this actually represents a combination ICE + DTLS
  // state, so it may be "failed" if DTLS fails while ICE succeeds.
  void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState
                                 /*new_state*/) noexcept override {}

  // Called any time the IceGatheringState changes.
  void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState
                                /*new_state*/) noexcept override {}

  // A new ICE candidate has been gathered.
  void OnIceCandidate(
      const webrtc::IceCandidateInterface* candidate) noexcept override;

 protected:
  // CreateSessionDescriptionObserver interface

  // This callback transfers the ownership of the |desc|.
  // TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
  // around ownership.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) noexcept override;

  // The OnFailure callback takes an RTCError, which consists of an
  // error code and a string.
  // RTCError is non-copyable, so it must be passed using std::move.
  // Earlier versions of the API used a string argument. This version
  // is deprecated; in order to let clients remove the old version, it has a
  // default implementation. If both versions are unimplemented, the
  // result will be a runtime error (stack overflow). This is intentional.
  void OnFailure(webrtc::RTCError error) noexcept override {}

 protected:
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_;
  ConnectedCallback connected_callback_;
  LocalSdpReadytoSendCallback local_sdp_ready_to_send_callback_;
  IceCandidateReadytoSendCallback ice_candidate_ready_to_send_callback_;
  RenegotiationNeededCallback renegotiation_needed_callback_;
  std::mutex connected_callback_mutex_;
  std::mutex local_sdp_ready_to_send_callback_mutex_;
  std::mutex ice_candidate_ready_to_send_callback_mutex_;
  std::mutex renegotiation_needed_callback_mutex_;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> local_video_track_;
  rtc::scoped_refptr<webrtc::AudioTrackInterface> local_audio_track_;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> local_video_sender_;
  rtc::scoped_refptr<webrtc::RtpSenderInterface> local_audio_sender_;
  std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>> remote_streams_;

  /// Collection of data channels from their unique ID.
  /// This contains only data channels pre-negotiated or opened by the remote
  /// peer, as data channels opened locally won't have immediately a unique ID.
  std::unordered_map<int, std::shared_ptr<DataChannelObserver>>
      data_channel_from_id_;

  /// Collection of data channels from their label.
  /// This contains only data channels with a non-empty label.
  std::unordered_multimap<std::string, std::shared_ptr<DataChannelObserver>>
      data_channel_from_label_;

  //< TODO - Clarify lifetime of those, for now same as this PeerConnection
  std::unique_ptr<AudioFrameObserver> local_audio_observer_;
  std::unique_ptr<AudioFrameObserver> remote_audio_observer_;
  std::unique_ptr<VideoFrameObserver> local_video_observer_;
  std::unique_ptr<VideoFrameObserver> remote_video_observer_;

 private:
  PeerConnection(const PeerConnection&) = delete;
  PeerConnection& operator=(const PeerConnection&) = delete;
};

}  // namespace webrtc_impl
}  // namespace net
}  // namespace mrtk
