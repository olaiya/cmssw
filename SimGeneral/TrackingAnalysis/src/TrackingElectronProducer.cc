#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/SiPixelDetId/interface/PXBDetId.h"
#include "DataFormats/SiPixelDetId/interface/PXFDetId.h"
#include "DataFormats/SiPixelDetId/interface/PixelSubdetector.h"
#include "DataFormats/SiStripDetId/interface/StripSubdetector.h"
#include "DataFormats/SiStripDetId/interface/TECDetId.h" 
#include "DataFormats/SiStripDetId/interface/TIBDetId.h" 
#include "DataFormats/SiStripDetId/interface/TIDDetId.h"
#include "DataFormats/SiStripDetId/interface/TOBDetId.h" 
#include "DataFormats/Common/interface/Handle.h"


#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "SimDataFormats/CrossingFrame/interface/CrossingFrame.h"
#include "SimDataFormats/CrossingFrame/interface/MixCollection.h"
#include "SimDataFormats/EncodedEventId/interface/EncodedEventId.h"
#include "SimDataFormats/HepMCProduct/interface/HepMCProduct.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticleFwd.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingVertexContainer.h"

#include "SimGeneral/TrackingAnalysis/interface/EncodedTruthId.h"
#include "SimGeneral/TrackingAnalysis/interface/TrackingElectronProducer.h"
#include "SimGeneral/TrackingAnalysis/interface/TkNavigableSimElectronAssembler.h"

#include <map>

using namespace edm;
using namespace std; 
using CLHEP::HepLorentzVector;

typedef TkNavigableSimElectronAssembler::VertexPtr VertexPtr;

TrackingElectronProducer::TrackingElectronProducer(const edm::ParameterSet &conf) {
  produces<TrackingVertexCollection>();
  produces<TrackingParticleCollection>();

  std::cout << " TrackingElectronProducer CTOR " << std::endl;


  conf_ = conf;
}


void TrackingElectronProducer::produce(Event &event, const EventSetup &) {

//  TimerStack timers;  // Don't need the timers now, left for example
//  timers.push("TrackingTruth:Producer");
//  timers.push("TrackingTruth:Setup");
  // Get information out of event record

  std::cout << " TrackingElectronProducer produce 1 " << std::endl;

  edm::Handle<TrackingParticleCollection>  TruthTrackContainer ;
  //  event.getByLabel("trackingtruthprod","TrackingTruthProducer", 
  //		   trackingParticleHandle);
  event.getByType(TruthTrackContainer );
  std::cout << " TrackingElectronProducer produce 1.5 " << std::endl;
  
  const TrackingParticleCollection *etPC   = TruthTrackContainer.product();
  std::cout << " TrackingElectronProducer produce 2 " << std::endl;

    // now dumping electrons only
  listElectrons(*etPC);

  std::cout << " TrackingElectronProducer produce 3 " << std::endl;
  // now calling electron assembler and dumping assembled electrons 
  cout << "TrackingElectronProducer::now assembling electrons..." << endl;
  TkNavigableSimElectronAssembler assembler;
  std::vector<TrackingParticle*> particles;
  for (TrackingParticleCollection::const_iterator t = etPC -> begin(); t != etPC -> end(); ++t) {
    particles.push_back(const_cast<TrackingParticle*>(&(*t)));
  }

  TkNavigableSimElectronAssembler::ElectronList 
    electrons = assembler.assemble(particles);

  std::cout << "Electron segments found, now linking them " << std::endl;

  //
  // now add electron tracks and vertices to event
  //
  // prepare collections
  //
  std::auto_ptr<TrackingParticleCollection> trackingParticles(new TrackingParticleCollection);
  std::auto_ptr<TrackingVertexCollection> trackingVertices(new TrackingVertexCollection);

  edm::RefProd<TrackingParticleCollection> refTPC =
    event.getRefBeforePut<TrackingParticleCollection>("ElectronTrackTruth");
  edm::RefProd<TrackingVertexCollection>   refTVC =
    event.getRefBeforePut<TrackingVertexCollection>("ElectronVertexTruth");

  //
  // create TrackingVertices
  //
  typedef std::map<TrackingVertex const *,int> TVMap;
  TVMap trackingVertexMap;
  int ntv(0);
  // loop over electrons
  for ( TkNavigableSimElectronAssembler::ElectronList::const_iterator ie 
	  = electrons.begin(); ie != electrons.end(); ie++ ) {

    // store parent vertex of first track segment 
    // and decay vertex of last track segment
    TrackingVertexRef parentV = (*(*ie).front()).parentVertex();
    if (parentV.isNonnull()) {
      (*trackingVertices).push_back(*parentV);
      trackingVertexMap[parentV.get()] = ntv++;
    }
    // for 131
    //    TrackingVertexRef decayV = (*(*ie).back()).decayVertex();
    
    TrackingVertexRefVector decayVertices = (*(*ie).back()).decayVertices();
    if ( !decayVertices.empty() ) {
      (*trackingVertices).push_back( *decayVertices.at(0) );
      trackingVertexMap[ &(*decayVertices.at(0)) ] = ntv++;
    }
  }
   
  //
  // create TrackingParticles
  //
  typedef std::map<TrackingParticle const *,int> TPMap;
  TPMap trackingParticleMap;
  int ntp(0);
  // loop over electrons
  for ( TkNavigableSimElectronAssembler::ElectronList::const_iterator ie 
	  = electrons.begin(); ie != electrons.end(); ie++ ) {

    // create TrackingParticle from first track segment 
    TrackingParticle * tk = (*ie).front();
    CLHEP::HepLorentzVector hep4Pos = (*(*tk).parentVertex()).position();
    TrackingParticle::Point pos(hep4Pos.x(), hep4Pos.y(), hep4Pos.z());
    TrackingParticle tkp( (*tk).charge(), (*tk).p4(), pos, hep4Pos.t(), 
			  (*tk).pdgId(), (*tk).eventId() );

    // add G4 tracks and hits of all segments
    for ( TkNavigableSimElectronAssembler::TrackList::const_iterator it 
	    = (*ie).begin(); it != (*ie).end(); it++ ) {
      addG4Track(tkp, *it);
    }
    (*trackingParticles).push_back(tkp);
    trackingParticleMap[tk] = ntp++;
  }

  //
  // add references to vertices
  //

  cout << "Dumping assembled electrons..." << endl;

  cout << "Storing electron tracks" << endl;
  event.put(trackingParticles,"ElectronTrackTruth");

} 


void TrackingElectronProducer::listElectrons(
  const TrackingParticleCollection & tPC) const
{
  cout << "TrackingElectronProducer::printing electrons before assembly..." 
       << endl;
  for (TrackingParticleCollection::const_iterator it = tPC.begin();
       it != tPC.end(); it++) {
    if (abs((*it).pdgId()) == 11) {
      cout << "Electron: sim tk " << (*it).g4Tracks().front().trackId() 
	   << endl;
      
      TrackingVertexRef parentV = (*it).parentVertex();
      if (parentV.isNull()) {
	cout << " No parent vertex" << endl;
      } else {  
	cout << " Parent  vtx position " << parentV -> position() << endl;
      }  
      
      TrackingVertexRefVector decayVertices = (*it).decayVertices();
      if ( decayVertices.empty() ) {
	cout << " No decay vertex" << endl;
      } else {  
	cout << " Decay vtx position " 
	     << decayVertices.at(0) -> position() << endl;
      } 
    }
  }
}


void 
TrackingElectronProducer::addG4Track(TrackingParticle& e, 
				     const TrackingParticle * s) const
{

  // add G4 tracks
  std::vector<SimTrack> g4Tracks = (*s).g4Tracks();
  for ( std::vector<SimTrack>::const_iterator ig4 = g4Tracks.begin(); 
	ig4 != g4Tracks.end(); ig4++ ) {
    e.addG4Track(*ig4);
  }

  // add hits 
  // FIXME configurable for dropping delta-ray hits
  std::vector< PSimHit > hits = (*s).trackPSimHit();
  for ( std::vector<PSimHit>::const_iterator ih = hits.begin(); 
	ih != hits.end(); ih++ ) {
    e.addPSimHit(*ih);
  }
}
  
// DEFINE_FWK_MODULE(TrackingElectronProducer);
