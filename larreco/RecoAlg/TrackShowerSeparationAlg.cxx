////////////////////////////////////////////////////////////////////////
// Class: TrackShowerSeparationAlg
// File:  TrackShowerSeparationAlg.cxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), November 2015
//
// Track/shower separation class.
// Provides methods for removing hits associated with track-like
// objects.
// To be run after track reconstruction, before shower reconstruction.
////////////////////////////////////////////////////////////////////////

#include "larreco/RecoAlg/TrackShowerSeparationAlg.h"

shower::TrackShowerSeparationAlg::TrackShowerSeparationAlg(fhicl::ParameterSet const& pset) {
  this->reconfigure(pset);
  // tree
  ftree = tfs->make<TTree>("tree","tree");
  ftree->Branch("Run",&Run);
  ftree->Branch("Event",&Event);
  ftree->Branch("Distance",&Distance);
  ftree->Branch("Angle",&Angle);
  ftree->Branch("Length",&Length);
  ftree->Branch("TrackID",&TrackID);
  ftree->Branch("PDG",&pdg);
  ftree->Branch("NSpacePoints",&NSpacePoints);
  ftree->Branch("AvDistance",&AvDistance);
}

void shower::TrackShowerSeparationAlg::reconfigure(fhicl::ParameterSet const& pset) {
  fAngleCut           = pset.get<double>("AngleCut");
  fDistanceCut        = pset.get<double>("DistanceCut");
  fVertexProximityCut = pset.get<double>("VertexProximityCut");
  fTrackProximityCut  = pset.get<double>("TrackProximityCut");
  fAvTrackHitDistance = pset.get<double>("AvTrackHitDistance");
}

std::vector<art::Ptr<recob::Hit> > shower::TrackShowerSeparationAlg::RemoveTrackHits(const std::vector<art::Ptr<recob::Hit> >& hits,
										     const std::vector<art::Ptr<recob::Track> >& tracks,
										     const art::FindManyP<recob::Hit>& fmh) {

  // Ok, here we are again
  // Playing the game in which no one wins, but everyone loses
  // Trying to separate tracks from showers
  //    Ode to track/shower separation
  //    M Wallbank, Oct 2016

  std::vector<int> showerLikeTracks, trackLikeTracks;
  std::vector<int> showerTracks = InitialTrackLikeSegment(tracks, showerLikeTracks, trackLikeTracks);

  std::cout << "Shower tracks are:" << std::endl;
  for (std::vector<int>::iterator showerTrackIt = showerTracks.begin(); showerTrackIt != showerTracks.end(); ++showerTrackIt)
    std::cout << *showerTrackIt << std::endl;

  // Shower hits
  std::vector<art::Ptr<recob::Hit> > showerHits;
  for (std::vector<int>::iterator showerLikeTrackIt = showerLikeTracks.begin(); showerLikeTrackIt != showerLikeTracks.end(); ++showerLikeTrackIt) {
    std::vector<art::Ptr<recob::Hit> > showerLikeHits = fmh.at(*showerLikeTrackIt);
    for (std::vector<art::Ptr<recob::Hit> >::iterator showerLikeHitIt = showerLikeHits.begin(); showerLikeHitIt != showerLikeHits.end(); ++showerLikeHitIt)
      showerHits.push_back(*showerLikeHitIt);
  }

  return showerHits;

}

std::vector<int> shower::TrackShowerSeparationAlg::InitialTrackLikeSegment(const std::vector<art::Ptr<recob::Track> >& tracks,
									   std::vector<int>& showerLikeTracks,
									   std::vector<int>& trackLikeTracks) {

  // Vector to hold tracks that are identified to be from the start of a shower
  std::vector<int> showerTrackCandidates;
  std::vector<std::vector<int> > showerLikeParts;

  const double angle = 20.;

  for (std::vector<art::Ptr<recob::Track> >::const_iterator trackIt = tracks.begin(); trackIt != tracks.end(); ++trackIt) {

    std::vector<int> forwardTracks, backwardTracks;
    
    for (std::vector<art::Ptr<recob::Track> >::const_iterator otherTrackIt = tracks.begin(); otherTrackIt != tracks.end(); ++otherTrackIt) {
      if (trackIt->key() == otherTrackIt->key())
	continue;
      if (((*otherTrackIt)->Vertex() - (*trackIt)->Vertex()).Angle((*trackIt)->DirectionAtPoint((*trackIt)->NumberTrajectoryPoints()/3)) < angle * TMath::Pi() / 180 or
	  ((*otherTrackIt)->End() - (*trackIt)->Vertex()).Angle((*trackIt)->DirectionAtPoint((*trackIt)->NumberTrajectoryPoints()/3)) < angle * TMath::Pi() / 180)
	forwardTracks.push_back(otherTrackIt->key());
      if (((*otherTrackIt)->Vertex() - (*trackIt)->Vertex()).Angle((-1)*(*trackIt)->DirectionAtPoint((*trackIt)->NumberTrajectoryPoints()/3)) < angle * TMath::Pi() / 180 or
	  ((*otherTrackIt)->End() - (*trackIt)->Vertex()).Angle((-1)*(*trackIt)->DirectionAtPoint((*trackIt)->NumberTrajectoryPoints()/3)) < angle * TMath::Pi() / 180)
	backwardTracks.push_back(otherTrackIt->key());
    }

    std::cout << "Track " << trackIt->key() << " has " << forwardTracks.size() << " forward tracks and " << backwardTracks.size() << " backward tracks" << std::endl;
    
    if ((int)forwardTracks.size() - (int)backwardTracks.size() > 5) {
      showerTrackCandidates.push_back(trackIt->key());
      std::cout << "Track " << trackIt->key() << " is a candidate" << std::endl;
      showerLikeParts.push_back(forwardTracks); //forwardTracks.size() > backwardTracks.size() ? forwardTracks : backwardTracks);
      for (std::vector<int>::iterator it = showerLikeParts.back().begin(); it != showerLikeParts.back().end(); ++it)
	std::cout << "  Track " << *it << " is in the forward part of this track" << std::endl;
    }

  }

  // Decide which tracks are shower track starts
  std::vector<int> showerTracks;

  for (std::vector<int>::iterator showerTrackCandidateIt = showerTrackCandidates.begin(); showerTrackCandidateIt != showerTrackCandidates.end(); ++showerTrackCandidateIt) {
    std::cout << "Looking at candidate " << *showerTrackCandidateIt << std::endl;
    bool notInShower = true;
    for (std::vector<std::vector<int> >::iterator showersLikePartsIt = showerLikeParts.begin(); showersLikePartsIt != showerLikeParts.end(); ++showersLikePartsIt)
      for (std::vector<int>::iterator showerLikePartsIt = showersLikePartsIt->begin(); showerLikePartsIt != showersLikePartsIt->end(); ++showerLikePartsIt)
	if (*showerLikePartsIt == *showerTrackCandidateIt) {
	  std::cout << "In the forward part of track " << *showerLikePartsIt << " -- can't be initial track like part" << std::endl;
	  notInShower = false;
	}
    if (notInShower)
      showerTracks.push_back(*showerTrackCandidateIt);
  }

  for (std::vector<int>::iterator showerTrackIt = showerTracks.begin(); showerTrackIt != showerTracks.end(); ++showerTrackIt) {
    for (std::vector<int>::iterator showerTrackCandidateIt = showerTrackCandidates.begin(); showerTrackCandidateIt != showerTrackCandidates.end(); ++showerTrackCandidateIt) {
      if (*showerTrackIt == *showerTrackCandidateIt) {
	showerLikeTracks.push_back(*showerTrackCandidateIt);
	for (std::vector<int>::iterator showerLikeTrackIt = showerLikeParts.at(std::distance(showerTrackCandidates.begin(),showerTrackCandidateIt)).begin();
	     showerLikeTrackIt != showerLikeParts.at(std::distance(showerTrackCandidates.begin(),showerTrackCandidateIt)).end();
	     ++showerLikeTrackIt)
	  if (std::find(showerLikeTracks.begin(), showerLikeTracks.end(), *showerLikeTrackIt) == showerLikeTracks.end())
	    showerLikeTracks.push_back(*showerLikeTrackIt);
      }
    }
  }

  std::cout << std::endl << "Here are the track determined to be showers" << std::endl;
  for (std::vector<int>::iterator showerTrackIt = showerLikeTracks.begin(); showerTrackIt != showerLikeTracks.end(); ++showerTrackIt)
    std::cout << "  " << *showerTrackIt << std::endl;
  std::cout << std::endl;

  return showerTracks;

}

// --------------------------- OLD (late 2015) -------------------------------


void shower::TrackShowerSeparationAlg::IdentifyTracksFromEventCentre(const std::vector<art::Ptr<recob::Track> >& tracks,
								     const std::vector<art::Ptr<recob::SpacePoint> >& spacePoints,
								     const art::FindManyP<recob::Track>& fmtsp) {

  // Find the charge weighted centre of the entire event!
  // ---- except space points don't have charge (could look at associated hits, but cba!)
  TVector3 centre = TVector3(0,0,0);
  for (std::vector<art::Ptr<recob::SpacePoint> >::const_iterator spacePointIt = spacePoints.begin(); spacePointIt != spacePoints.end(); ++spacePointIt)
    centre += TVector3((*spacePointIt)->XYZ()[0], (*spacePointIt)->XYZ()[1], (*spacePointIt)->XYZ()[2]);
  centre *= 1/(double)spacePoints.size();

  TVector3 trackVertex, trackEnd, trackDirection;

  // Look at all tracks
  for (std::vector<art::Ptr<recob::Track> >::const_iterator trackIt = tracks.begin(); trackIt != tracks.end(); ++trackIt) {

    trackVertex = ( ((*trackIt)->Vertex()-centre).Mag() > ((*trackIt)->End()-centre).Mag() ) ? (*trackIt)->Vertex() : (*trackIt)->End();
    trackEnd = ( ((*trackIt)->Vertex()-centre).Mag() > ((*trackIt)->End()-centre).Mag() ) ? (*trackIt)->End() : (*trackIt)->Vertex();
    trackDirection = ( ((*trackIt)->Vertex()-centre).Mag() > ((*trackIt)->End()-centre).Mag() ) ? (*trackIt)->VertexDirection() : (-1)*(*trackIt)->EndDirection();

    // Get the space points not associated with the current track
    std::vector<art::Ptr<recob::SpacePoint> > surroundingSpacePoints = GetSurroundingSpacePoints(spacePoints, fmtsp, (*trackIt)->ID());

    // // TRUE -- use truth to find which particle this track is associated with --------
    // std::vector<art::Ptr<recob::Hit> > trackHits = fmh.at(trackIt->key());
    // int trueTrackID = FindTrueTrack(trackHits);
    // const simb::MCParticle* trueParticle = backtracker->TrackIDToParticle(trueTrackID);
    // pdg = trueParticle->PdgCode();
    // Length = (*trackIt)->Length();
    // TrackID = (*trackIt)->ID();
    // // -------------------------------------------------------------------------------

    bool showerLike = IdentifyShowerLikeTrack(trackEnd, trackDirection, surroundingSpacePoints);

    if (showerLike) {
      fShowerLikeIDs.push_back((*trackIt)->ID());
      continue;
    }

    else
      fTrackLikeIDs.push_back((*trackIt)->ID());

  }

  return;

}

void shower::TrackShowerSeparationAlg::IdentifyTracksNearTracks(const std::vector<art::Ptr<recob::Track> >& tracks) {

  std::vector<art::Ptr<recob::Track> > identifiedTracks;

  for (std::vector<art::Ptr<recob::Track> >::const_iterator trackIt = tracks.begin(); trackIt != tracks.end(); ++trackIt)
    if (std::find(fTrackLikeIDs.begin(), fTrackLikeIDs.end(), (*trackIt)->ID()) != fTrackLikeIDs.end())
      identifiedTracks.push_back(*trackIt);

  // Look through tracks
  bool allTracksRemoved = false;
  while (!allTracksRemoved) {

    int tracksRemoved = 0;

    // Look through all the tracks
    for (std::vector<art::Ptr<recob::Track> >::const_iterator trackIt = tracks.begin(); trackIt != tracks.end(); ++trackIt) {

      // Don't consider tracks already tagged as tracks!
      if (std::find(fTrackLikeIDs.begin(), fTrackLikeIDs.end(), (*trackIt)->ID()) != fTrackLikeIDs.end())
	continue;

      bool trackShouldBeRemoved = false;

      // Find tracks which are close to previously identified tracks
      for (std::vector<art::Ptr<recob::Track> >::iterator identifiedTrackIt = identifiedTracks.begin(); identifiedTrackIt != identifiedTracks.end(); ++identifiedTrackIt) {
	if (std::find(fShowerLikeIDs.begin(), fShowerLikeIDs.end(), (*trackIt)->ID()) != fShowerLikeIDs.end())
	  continue;
	if ( ( ((*trackIt)->Vertex() - (*identifiedTrackIt)->Vertex()).Mag() < fTrackProximityCut ) or
	     ( ((*trackIt)->Vertex() - (*identifiedTrackIt)->End()   ).Mag() < fTrackProximityCut ) or
	     ( ((*trackIt)->End()    - (*identifiedTrackIt)->Vertex()).Mag() < fTrackProximityCut ) or
	     ( ((*trackIt)->End()    - (*identifiedTrackIt)->End()   ).Mag() < fTrackProximityCut ) )
	  trackShouldBeRemoved = true;
      }

      // Tag this track as a 'track-like' object
      if (trackShouldBeRemoved) {
	fTrackLikeIDs.push_back((*trackIt)->ID());
	identifiedTracks.push_back(*trackIt);
	++tracksRemoved;
      }

    }

    // If there were no tracks removed then we'll call it a day
    if (tracksRemoved == 0)
      allTracksRemoved = true;

  }

  return;

}

void shower::TrackShowerSeparationAlg::IdentifyTracksNearVertex(const art::Ptr<recob::Vertex>& vertex,
								const std::vector<art::Ptr<recob::Track> >& tracks,
								const std::vector<art::Ptr<recob::SpacePoint> >& spacePoints,
								const art::FindManyP<recob::Track>& fmtsp) {

  double xyz[3];
  vertex->XYZ(xyz);
  TVector3 vertexPos = TVector3(xyz[0], xyz[1], xyz[2]);

  // Look at each track
  for (std::vector<art::Ptr<recob::Track> >::const_iterator trackIt = tracks.begin(); trackIt != tracks.end(); ++trackIt) {

    TVector3 end, direction;

    // See if either end is close to the vertex
    if ( ((*trackIt)->Vertex() - vertexPos).Mag() < fVertexProximityCut or
	 ((*trackIt)->End() - vertexPos).Mag() < fVertexProximityCut) {
      end = ((*trackIt)->Vertex() - vertexPos).Mag() < ((*trackIt)->End() - vertexPos).Mag() ? (*trackIt)->End() : (*trackIt)->VertexDirection();
      direction = ((*trackIt)->Vertex() - vertexPos).Mag() < ((*trackIt)->End() - vertexPos).Mag() ? (*trackIt)->VertexDirection() : (-1)*(*trackIt)->VertexDirection();
    }

    else
      continue;

    // Get the space points not associated with the current track
    std::vector<art::Ptr<recob::SpacePoint> > surroundingSpacePoints = GetSurroundingSpacePoints(spacePoints, fmtsp, (*trackIt)->ID());

    // Make sure this track start isn't a shower start
    bool showerLike = IdentifyShowerLikeTrack(end, direction, surroundingSpacePoints);

    if (showerLike) {
      fShowerLikeIDs.push_back((*trackIt)->ID());
      continue;
    }

    // This track originates from near the interaction vertex and is not shower-like
    fTrackLikeIDs.push_back((*trackIt)->ID());

  }

  return;

}

bool shower::TrackShowerSeparationAlg::IdentifyShowerLikeTrack(const TVector3& end,
							       const TVector3& direction,
							       const std::vector<art::Ptr<recob::SpacePoint> >& spacePoints) {

  std::vector<art::Ptr<recob::SpacePoint> > spacePointsInCone = GetSpacePointsInCone(spacePoints, end, direction);

  if (spacePointsInCone.size() < 2)
    return false;

  // Get the average spread of these space points
  double spread = SpacePointSpread(spacePointsInCone);

  if (spread < fAvTrackHitDistance)
    return false;

  return true;

}

std::vector<art::Ptr<recob::Hit> > shower::TrackShowerSeparationAlg::FillHitsToCluster(const std::vector<art::Ptr<recob::Hit> >& initialHits,
										       const art::FindManyP<recob::Track>& fmt) {

  // Container to fill with shower-like hits
  std::vector<art::Ptr<recob::Hit> > hitsToCluster;

  for (std::vector<art::Ptr<recob::Hit> >::const_iterator initialHit = initialHits.begin(); initialHit != initialHits.end(); ++initialHit) {
    std::vector<art::Ptr<recob::Track> > showerTracks = fmt.at(initialHit->key());
    if ( (showerTracks.size() and (std::find(fTrackLikeIDs.begin(), fTrackLikeIDs.end(), showerTracks.at(0)->ID()) == fTrackLikeIDs.end()))//hit on track-like track
	 || !showerTracks.size() )//hit not on any track
      hitsToCluster.push_back(*initialHit);
  }

  return hitsToCluster;

}

int shower::TrackShowerSeparationAlg::FindTrackID(const art::Ptr<recob::Hit>& hit) {
  double particleEnergy = 0;
  int likelyTrackID = 0;
  std::vector<sim::TrackIDE> trackIDs = backtracker->HitToTrackID(hit);
  for (unsigned int idIt = 0; idIt < trackIDs.size(); ++idIt) {
    if (trackIDs.at(idIt).energy > particleEnergy) {
      particleEnergy = trackIDs.at(idIt).energy;
      likelyTrackID = TMath::Abs(trackIDs.at(idIt).trackID);
    }
  }
  return likelyTrackID;
}

int shower::TrackShowerSeparationAlg::FindTrueTrack(const std::vector<art::Ptr<recob::Hit> >& trackHits) {
  std::map<int,double> trackMap;
  for (std::vector<art::Ptr<recob::Hit> >::const_iterator trackHitIt = trackHits.begin(); trackHitIt != trackHits.end(); ++trackHitIt) {
    art::Ptr<recob::Hit> hit = *trackHitIt;
    int trackID = FindTrackID(hit);
    trackMap[trackID] += hit->Integral();
  }
  //return std::max_element(trackMap.begin(), trackMap.end(), [](const std::pair<int,double>& p1, const std::pair<int,double>& p2) {return p1.second < p2.second;} )->first;
  double highestCharge = 0;
  int clusterTrack = 0;
  for (std::map<int,double>::iterator trackIt = trackMap.begin(); trackIt != trackMap.end(); ++trackIt)
    if (trackIt->second > highestCharge) {
      highestCharge = trackIt->second;
      clusterTrack  = trackIt->first;
    }
  return clusterTrack;
}

std::vector<art::Ptr<recob::SpacePoint> > shower::TrackShowerSeparationAlg::GetSurroundingSpacePoints(const std::vector<art::Ptr<recob::SpacePoint> >& spacePoints,
												      const art::FindManyP<recob::Track>& fmt,
												      unsigned int trackID) {

  // The space points to return
  std::vector<art::Ptr<recob::SpacePoint> > surroundingSpacePoints;

  for (std::vector<art::Ptr<recob::SpacePoint> >::const_iterator spacePointIt = spacePoints.begin(); spacePointIt != spacePoints.end(); ++spacePointIt) {

    bool spacePointIsInCurrentTrack = false;
  
    std::vector<art::Ptr<recob::Track> > spacePointTracks = fmt.at(spacePointIt->key());
    for (std::vector<art::Ptr<recob::Track> >::iterator spacePointTrackIt = spacePointTracks.begin(); spacePointTrackIt != spacePointTracks.end(); ++spacePointTrackIt)
      if (spacePointTrackIt->key() == trackID) spacePointIsInCurrentTrack = true;
    
    if (!spacePointIsInCurrentTrack)
      surroundingSpacePoints.push_back(*spacePointIt);

  }

  return surroundingSpacePoints;

}

std::vector<art::Ptr<recob::SpacePoint> > shower::TrackShowerSeparationAlg::GetSpacePointsInCone(const std::vector<art::Ptr<recob::SpacePoint> >& spacePoints,
												 const TVector3& trackEnd,
												 const TVector3& trackDirection) {

  // The space points in cone to return
  std::vector<art::Ptr<recob::SpacePoint> > spacePointsInCone;

  TVector3 spacePointPos, spacePointProj;
  double displacementFromAxis, distanceFromTrackEnd, projDistanceFromTrackEnd, angleFromTrackEnd;

  for (std::vector<art::Ptr<recob::SpacePoint> >::const_iterator spacePointIt = spacePoints.begin(); spacePointIt != spacePoints.end(); ++spacePointIt) {

    // Get the properties of this space point
    const double* xyz = (*spacePointIt)->XYZ();
    spacePointPos = TVector3(xyz[0], xyz[1], xyz[2]);
    spacePointProj = ((spacePointPos-trackEnd).Dot(trackDirection))*trackDirection + trackEnd;
    displacementFromAxis = (spacePointProj - spacePointPos).Mag();
    distanceFromTrackEnd = (spacePointPos - trackEnd).Mag();
    projDistanceFromTrackEnd = (spacePointProj - trackEnd).Mag();
    angleFromTrackEnd = TMath::ASin(displacementFromAxis/distanceFromTrackEnd) * 180 / TMath::Pi();

    if ( (projDistanceFromTrackEnd < fDistanceCut) and (angleFromTrackEnd < fAngleCut) and ((spacePointProj-trackEnd).Dot(trackDirection) > 0) )
      spacePointsInCone.push_back(*spacePointIt);

  }

  return spacePointsInCone;

}

std::vector<art::Ptr<recob::Hit> > shower::TrackShowerSeparationAlg::RemoveTrackHits(const std::vector<art::Ptr<recob::Hit> >& initialHits,
										     const std::vector<art::Ptr<recob::Track> >& tracks,
										     const std::vector<art::Ptr<recob::SpacePoint> >& spacePoints,
										     const std::vector<art::Ptr<recob::Vertex> >& vertices,
										     const art::FindManyP<recob::Track>& fmth,
										     const art::FindManyP<recob::Track>& fmtsp,
										     const art::FindManyP<recob::Hit>& fmh,
										     int event,
										     int run) {

  Event = event;
  Run = run;

  if (spacePoints.size() == 0)
    return initialHits;

  // Container for shower-like hits
  std::vector<art::Ptr<recob::Hit> > hitsToCluster;

  fTrackLikeIDs.clear();
  fShowerLikeIDs.clear();

  // Find the vertex furthest upstream (if it exists)
  art::Ptr<recob::Vertex> vertex;
  for (std::vector<art::Ptr<recob::Vertex> >::const_iterator vertexIt = vertices.begin(); vertexIt != vertices.end(); ++vertexIt) {
    double xyzNew[3], xyzOld[3];
    (*vertexIt)->XYZ(xyzNew);
    if (vertex.isNull())
      vertex = *vertexIt;
    else {
      vertex->XYZ(xyzOld);
      if (xyzNew[2] < xyzOld[2])
	vertex = *vertexIt;
    }
  }

  // If we have a vertex then, for the time being, use it to remove tracks which are too close!
  if (!vertex.isNull())
    this->IdentifyTracksNearVertex(vertex, tracks, spacePoints, fmtsp);

  // If no vertex, things are harder
  else
    this->IdentifyTracksFromEventCentre(tracks, spacePoints, fmtsp);

  // Once we've identified some tracks, can look for others at the ends
  this->IdentifyTracksNearTracks(tracks);

  hitsToCluster = FillHitsToCluster(initialHits, fmth);

  return hitsToCluster;

}

std::vector<art::Ptr<recob::Hit> > shower::TrackShowerSeparationAlg::RemoveTrackHits(const std::vector<art::Ptr<recob::Hit> >& hits,
										     const std::vector<art::Ptr<recob::PFParticle> > pfParticles,
										     const art::FindManyP<recob::Cluster>& fmc,
										     const art::FindManyP<recob::Hit>& fmh) {

  std::vector<art::Ptr<recob::Hit> > showerHits;

  // Use information from Pandora to identify shower-like hits
  for (std::vector<art::Ptr<recob::PFParticle> >::const_iterator pfParticleIt = pfParticles.begin(); pfParticleIt != pfParticles.end(); ++pfParticleIt) {

    // See if this is a shower particle
    if ((*pfParticleIt)->PdgCode() == 11) {
      std::vector<art::Ptr<recob::Cluster> > clusters = fmc.at(pfParticleIt->key());
      for (std::vector<art::Ptr<recob::Cluster> >::iterator clusterIt = clusters.begin(); clusterIt != clusters.end(); ++clusterIt) {
        std::vector<art::Ptr<recob::Hit> > hits = fmh.at(clusterIt->key());
        for (std::vector<art::Ptr<recob::Hit> >::iterator hitIt = hits.begin(); hitIt != hits.end(); ++hitIt)
          showerHits.push_back(*hitIt);
      }
    }

  }

  return showerHits;

}

double shower::TrackShowerSeparationAlg::SpacePointSpread(const std::vector<art::Ptr<recob::SpacePoint> >& spacePoints) {

  /// Finds the spread of a set of space points about their central axis

  // Find the centre of the space points
  TVector3 centre = TVector3(0,0,0);
  for (std::vector<art::Ptr<recob::SpacePoint> >::const_iterator spacePointIt = spacePoints.begin(); spacePointIt != spacePoints.end(); ++spacePointIt)
    centre += TVector3((*spacePointIt)->XYZ()[0], (*spacePointIt)->XYZ()[1], (*spacePointIt)->XYZ()[2]);
  centre *= 1/(double)spacePoints.size();

  // Find the central axis of the space points
  TPrincipal* pca = new TPrincipal(3,"");
  for (std::vector<art::Ptr<recob::SpacePoint> >::const_iterator spacePointIt = spacePoints.begin(); spacePointIt != spacePoints.end(); ++spacePointIt)
    pca->AddRow((*spacePointIt)->XYZ());
  pca->MakePrincipals();
  const TMatrixD* eigenvectors = pca->GetEigenVectors();
  TVector3 spacePointDir = TVector3((*eigenvectors)[0][0], (*eigenvectors)[1][0], (*eigenvectors)[2][0]);
  delete pca;

  // See if the space points form something which may resemble a track (i.e. straight line) or a shower
  double avDistanceFromCentralAxis = 0;
  for (std::vector<art::Ptr<recob::SpacePoint> >::const_iterator spacePointIt = spacePoints.begin(); spacePointIt != spacePoints.end(); ++spacePointIt) {
    const double* xyz = (*spacePointIt)->XYZ();
    TVector3 spacePointPos = TVector3(xyz[0], xyz[1], xyz[2]);
    TVector3 projectionOntoCentralAxis = ((spacePointPos-centre).Dot(spacePointDir))*spacePointDir + centre;
    avDistanceFromCentralAxis += (spacePointPos - projectionOntoCentralAxis).Mag();
  }
  avDistanceFromCentralAxis /= spacePoints.size();

  return avDistanceFromCentralAxis;

}
