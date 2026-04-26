
#include "inclusion.h"

#include "TargetOf.h"
#include "FixpointLoop.h"
#include "FixpointStem.h"
#include "ContextOf.h"
#include "StateRelation.h"

#include <chrono>


bool relevance_test (TargetOf* W, ContextOf* V, const Automaton* B) {
	for (unsigned int weight_id = 0; weight_id < V->size(); ++weight_id) {
		for (std::pair<State*, TargetOf*> pair : *(V->at(weight_id, false))) { // FRESH: at is parameterized with false
			if ((pair.second)->smaller_than(W) == false) return false;
		}
	}
	return true;
}




//////////////////////////////////////////// FRESH membership

SetStd<std::pair<State*,std::pair<unsigned int, int>>> S;
SetStd<std::pair<State*,std::pair<unsigned int, bool>>> C;


bool membership_query_final_cycle (
	State* root,
	unsigned int root_i,
	State* from,
	unsigned int i,
	Word* period,
	weight_t threshold,
	bool seen_threshold
) {
	std::pair<State*,std::pair<unsigned int, bool>> key(from, std::pair<unsigned int, bool>(i, seen_threshold));
	if (C.contains(key)) return false;
	C.insert(key);

	for (Edge* edge : *(from->getSuccessors(period->at(i)->getId()))) {
		unsigned int ii = ((i+1 == period->getLength()) ? 0 : i+1);
		bool next_seen_threshold = seen_threshold || edge->getWeight()->getValue() >= threshold;

		if (next_seen_threshold && edge->getTo() == root && ii == root_i) return true;
		if (membership_query_final_cycle(root, root_i, edge->getTo(), ii, period, threshold, next_seen_threshold) == true) return true;
	}

	return false;
}



bool membership_query_dfs2 (State* from, unsigned int i, Word* period, weight_t threshold) {
	C.clear();
	return membership_query_final_cycle(from, i, from, i, period, threshold, false);
}


bool membership_query_dfs1 (State* from, unsigned int i, Word* period, weight_t threshold) {
	S.insert(std::pair<State*,std::pair<unsigned int, int>>(from, std::pair<unsigned int, int>(i, 1)));

	for (Edge* edge : *(from->getSuccessors(period->at(i)->getId()))) {
		unsigned int ii = ((i+1 == period->getLength()) ? 0 : i+1);
		if (S.contains(std::pair<State*,std::pair<unsigned int, int>>(edge->getTo(), std::pair<unsigned int, int>(ii, 1))) == false) {
			if (membership_query_dfs1(edge->getTo(), ii, period, threshold) == true) return true;
		}
	}

	if (from->getFinal() == true) {
		if (membership_query_dfs2(from, i, period, threshold) == true) return true;
	}

	return false;
}



bool fast_membership (TargetOf* U, Word* period, weight_t threshold) {
	S.clear();
	for (State* start : *U) {
		if (membership_query_dfs1(start, 0, period, threshold) == true) return true;
	}
	return false;
}


bool membership (Automaton* A, Word* stem, Word* period, weight_t threshold) {
	TargetOf* current_target = new TargetOf();
	current_target->add(A->getInitial());

	TargetOf* post_target;
	for (unsigned int i = 0; i < stem->getLength(); i++) {
		post_target = new TargetOf(current_target, stem->at(i));
		delete current_target;
		current_target = post_target;
	}

	return fast_membership(current_target, period, threshold);
}



//////////////////////////////////////////

/*

SetStd<std::pair<State*,std::pair<unsigned int, bool>>> S;
SetStd<std::pair<State*,unsigned int>> P;





bool fast_iterable_final_product (State* from, unsigned int i, Word* period) {
	S.insert(std::pair<State*,std::pair<unsigned int, bool>>(from, std::pair<unsigned int, bool>(i, true)));

	for (Edge* edge : *(from->getSuccessors(period->at(i)->getId()))) {
		unsigned int ii = ((i+1 == period->getLength()) ? 0 : i+1);
		if (P.contains(std::pair<State*, unsigned int>(edge->getTo(), ii))) return true;
		if (S.contains(std::pair<State*,std::pair<unsigned int, bool>>(edge->getTo(), std::pair<unsigned int, bool>(ii, true))) == false) {
			if (fast_iterable_final_product(edge->getTo(), ii, period) == true) return true;
		}
	}

	return false;
}



bool fast_reachable_final_product (State* from, unsigned int i, Word* period, weight_t threshold) {
	S.insert(std::pair<State*,std::pair<unsigned int, bool>>(from, std::pair<unsigned int, bool>(i, false)));
	P.insert(std::pair<State*, unsigned int>(from, i));

	for (Edge* edge : *(from->getSuccessors(period->at(i)->getId()))) {
		unsigned int ii = ((i+1 == period->getLength()) ? 0 : i+1);
		if (S.contains(std::pair<State*,std::pair<unsigned int, bool>>(edge->getTo(), std::pair<unsigned int, bool>(ii, false))) == false) {
			if (fast_reachable_final_product(edge->getTo(), ii, period, threshold) == true) return true;
		}
	}

	for (Edge* edge : *(from->getSuccessors(period->at(i)->getId()))) {
		if (edge->getWeight()->getValue() >= threshold) {
			unsigned int ii = ((i+1 == period->getLength()) ? 0 : i+1);
			if (P.contains(std::pair<State*, unsigned int>(edge->getTo(), ii))) return true;
			if (S.contains(std::pair<State*,std::pair<unsigned int, bool>>(edge->getTo(), std::pair<unsigned int, bool>(ii, true))) == false) {
				if (fast_iterable_final_product(edge->getTo(), ii, period) == true) return true;
			}
		}
	}

	P.erase(std::pair<State*, unsigned int>(from, i));

	return false;
}



bool fast_membership (TargetOf* U, Word* period, weight_t threshold) {
	for (State* start : *U) {
		S.clear(); P.clear();
		if (fast_reachable_final_product(start, 0, period, threshold) == true) return true;
	}
	return false;
}

*/

//////////////////////////////////////////////////



bool inclusion (const Automaton* A, const Automaton* B, UltimatelyPeriodicWord** witness)  {

	FixpointStem* postIrev = new FixpointStem(A->getInitial(), B->getInitial(), true);
	while (postIrev->apply()) {
	}
	
	/*
	for (unsigned int stateA_id = 0; stateA_id < A->getStates()->size(); ++stateA_id) {
		State* stateA = A->getStates()->at(stateA_id);
		SetStd<std::pair<TargetOf*, Word*>>* targets = postIrev->getSetOfTargetsOrNULL(stateA);
		printf("%s ->\n", stateA->getName().c_str());
		for (std::pair<TargetOf*, Word*> pair : *targets) {
			printf("\t '%s'\n", pair.second->toString().c_str());
		}
	}
	*/


	FixpointStem* postI = new FixpointStem(A->getInitial(), B->getInitial(), false);
	while (postI->apply()) {
	}

	/*
	for (unsigned int stateA_id = 0; stateA_id < A->getStates()->size(); ++stateA_id) {
		State* stateA = A->getStates()->at(stateA_id);
		SetStd<std::pair<TargetOf*, Word*>>* targets = postI->getSetOfTargetsOrNULL(stateA);
		printf("%s ->\n", stateA->getName().c_str());
		for (std::pair<TargetOf*, Word*> pair : *targets) {
			printf("\t '%s'\n", pair.second->toString().c_str());
		}
	}
	*/


	for (unsigned int stateA_id = 0; stateA_id < A->getStates()->size(); ++stateA_id) {
		State* stateA = A->getStates()->at(stateA_id);

		if (stateA->getFinal() == false) continue; //FRESH: added condition
		if (stateA->getMaxWeightValue() <= B->getMinDomain()) continue;


		SetStd<std::pair<TargetOf*, Word*>>* setW = postIrev->getSetOfTargetsOrNULL(stateA);
		if (setW == nullptr) continue;
		for (std::pair<TargetOf*, Word*> pairW : *setW) {
	
			TargetOf* W = pairW.first;
	//		Word* word_of_W = pairW.second; // unused
			

			for (Symbol* symbol : *(stateA->getAlphabet())) { // FRESH: Why loop symbol not in FixpointLoop constructor?
				
				FixpointLoop* postF = new FixpointLoop(symbol, stateA, W, B->getWeights()->size());
				while (postF->apply()) {
				}
				
				SetStd<std::pair<ContextOf*, std::pair<Word*,weight_t>>>* setV = postF->getSetOfContextsOrNULL(stateA);
				if (setV == nullptr) {
					delete postF;
					continue;
				}

				for (std::pair<ContextOf*, std::pair<Word*,weight_t>> pairV : *setV) {
					ContextOf* V = pairV.first;
					Word* word_of_V = pairV.second.first;
					weight_t valueA = pairV.second.second;


					if (relevance_test(W, V, B) == false) continue;
					SetStd<std::pair<TargetOf*, Word*>>* setU = postI->getSetOfTargetsOrNULL(stateA);
					if (setU == NULL) continue;

					for (std::pair<TargetOf*, Word*> pairU : *setU) {
						TargetOf* U = pairU.first;
						Word* word_of_U = pairU.second;

						if (U->smaller_than(W) == true) {


							if (fast_membership(U, word_of_V, valueA) == false) {
	//							printf("witness: %s cycle{ %s }\n", word_of_U->toString().c_str(), word_of_V->toString().c_str());
	//							printf("FALSE\n");
								if (witness != nullptr) {
                                    *witness = new UltimatelyPeriodicWord{new Word(*word_of_U), new Word(*word_of_V)};
								}
								delete postF;
								delete postI;
								delete postIrev;
								return false;
							}
						}
					}
				}
				delete postF;
			}
		}
	}
	delete postI;
	delete postIrev;

	//printf("TRUE\n");
	return true;
}


void debug_test2() {
	// Automaton* toto;
	// Automaton* titi;

	// printf("--------------------------------------------\n");
	// printf("--------------------------------------------\n");
 	
	// std::cout << "All_positive_numbers_have_a_predecessor" << std::endl;
	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_positive_numbers_have_a_predecessor_SUBSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_positive_numbers_have_a_predecessor_SUPERSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// printf("--------------------------------------------\n");

	// std::cout << "All_Sturmian_words_contain_cubes" << std::endl;
	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_contain_cubes_SUBSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_contain_cubes_SUPERSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// printf("--------------------------------------------\n");

	// std::cout << "All_Sturmian_words_start_with_arbitarily_long_palindromes" << std::endl;
	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_start_with_arbitarily_long_palindromes_SUBSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_start_with_arbitarily_long_palindromes_SUPERSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// printf("--------------------------------------------\n");

	// std::cout << "bakeryV3" << std::endl;
	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/bakeryV3_SUBSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/bakeryV3_SUPERSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// printf("--------------------------------------------\n");

	// std::cout << "bigb" << std::endl;
	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/bigb_SUBSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/bigb_SUPERSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;
	
	// printf("--------------------------------------------\n");

	// std::cout << "BuchiCegarLoopAbstraction_SUBSET" << std::endl;
	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/BuchiCegarLoopAbstraction_SUBSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;

	// toto = Automaton::from_file_sync_alphabet("../samples/boolean/BuchiCegarLoopAbstraction_SUPERSET.txt");
	// titi = Automaton::determinizeInf(toto);
	// std::cout << toto->isIncludedIn(titi, Inf) << " " << titi->isIncludedIn(toto, Inf);

	// delete toto;
	// delete titi;




	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");

	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUBSET.txt");
	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUPERSET.txt", toto);
	//  std::cout << "example" << std::endl;
	//  inclusion(toto, titi);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");

	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUPERSET.txt");
	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUBSET.txt", titi);
	//  std::cout << "example" << std::endl;
	//  inclusion(titi, toto);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");

	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUBSET.txt");
	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUPERSET.txt", toto);
	//  std::cout << "fischerV3" << std::endl;
	//  std::cout << "NOT COMPUTED" << std::endl;//inclusion(toto, titi);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");

	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUPERSET.txt");
	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUBSET.txt", titi);
	//  std::cout << "fischerV3" << std::endl;
	//  std::cout << "NOT COMPUTED" << std::endl;//inclusion(titi, toto);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");

	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUBSET.txt");
	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUPERSET.txt", toto);
	//  std::cout << "Odd_and_even_work_as_expected" << std::endl;
	//  std::cout << "NOT COMPUTED" << std::endl;//inclusion(toto, titi);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");

	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUPERSET.txt");
	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUBSET.txt", titi);
	//  std::cout << "Odd_and_even_work_as_expected" << std::endl;
	//  std::cout << "NOT COMPUTED" << std::endl;//inclusion(titi, toto);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");

	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUBSET.txt");
	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUPERSET.txt", toto);
	//  std::cout << "peterson" << std::endl;
	//  inclusion(toto, titi);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");

	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUPERSET.txt");
	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUBSET.txt", titi);
	//  std::cout << "peterson" << std::endl;
	//  inclusion(titi, toto);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");

	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUBSET.txt");
	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUPERSET.txt", toto);
	//  std::cout << "slides_example" << std::endl;
	//  inclusion(toto, titi);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");

	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUPERSET.txt");
	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUBSET.txt", titi);
	//  std::cout << "slides_example" << std::endl;
	//  inclusion(titi, toto);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");

	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUBSET.txt");
	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUPERSET.txt", toto);
	//  std::cout << "Specal_factors_are_unique" << std::endl;
	//  inclusion(toto, titi);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");

	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUPERSET.txt");
	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUBSET.txt", titi);
	//  std::cout << "Specal_factors_are_unique" << std::endl;
	//  inclusion(titi, toto);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");

	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUBSET.txt");
	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUPERSET.txt", toto);
	//  std::cout << "The_lazy_Ostrowski_representation_is_unique" << std::endl;
	//  inclusion(toto, titi);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");

	//  titi = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUPERSET.txt");
	//  toto = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUBSET.txt", titi);
	//  std::cout << "The_lazy_Ostrowski_representation_is_unique" << std::endl;
	//  inclusion(titi, toto);
	//  delete toto;
	//  delete titi;

	//  printf("--------------------------------------------\n");
	//  printf("--------------------------------------------\n");
}


void debug_test() {
	Automaton* toto;
	Automaton* titi;

	printf("--------------------------------------------\n");
	printf("--------------------------------------------\n");

	toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_positive_numbers_have_a_predecessor_SUBSET.txt");
	titi = Automaton::from_file_sync_alphabet("../samples/boolean/All_positive_numbers_have_a_predecessor_SUPERSET.txt", toto);
	std::cout << "All_positive_numbers_have_a_predecessor" << std::endl;
	inclusion(toto, titi);
	delete toto;
	delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/All_positive_numbers_have_a_predecessor_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_positive_numbers_have_a_predecessor_SUBSET.txt", titi);
	 std::cout << "All_positive_numbers_have_a_predecessor" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_contain_cubes_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_contain_cubes_SUPERSET.txt", toto);
	 std::cout << "All_Sturmian_words_contain_cubes" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_contain_cubes_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_contain_cubes_SUBSET.txt", titi);
	 std::cout << "All_Sturmian_words_contain_cubes" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_start_with_arbitarily_long_palindromes_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_start_with_arbitarily_long_palindromes_SUPERSET.txt", toto);
	 std::cout << "All_Sturmian_words_start_with_arbitarily_long_palindromes" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_start_with_arbitarily_long_palindromes_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/All_Sturmian_words_start_with_arbitarily_long_palindromes_SUBSET.txt", titi);
	 std::cout << "All_Sturmian_words_start_with_arbitarily_long_palindromes" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/bakeryV3_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/bakeryV3_SUPERSET.txt", toto);
	 std::cout << "bakeryV3" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/bakeryV3_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/bakeryV3_SUBSET.txt", titi);
	 std::cout << "bakeryV3" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/bigb_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/bigb_SUPERSET.txt", toto);
	 std::cout << "bigb" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/bigb_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/bigb_SUBSET.txt", titi);
	 std::cout << "bigb" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/BuchiCegarLoopAbstraction_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/BuchiCegarLoopAbstraction_SUPERSET.txt", toto);
	 std::cout << "BuchiCegarLoopAbstraction" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/BuchiCegarLoopAbstraction_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/BuchiCegarLoopAbstraction_SUBSET.txt", titi);
	 std::cout << "BuchiCegarLoopAbstraction" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUPERSET.txt", toto);
	 std::cout << "example" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/example_SUBSET.txt", titi);
	 std::cout << "example" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUPERSET.txt", toto);
	 std::cout << "fischerV3" << std::endl;
	 std::cout << "NOT COMPUTED" << std::endl;//inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUBSET.txt", titi);
	 std::cout << "fischerV3" << std::endl;
	 std::cout << "NOT COMPUTED" << std::endl;//inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUPERSET.txt", toto);
	 std::cout << "Odd_and_even_work_as_expected" << std::endl;
	 std::cout << "NOT COMPUTED" << std::endl;//inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUBSET.txt", titi);
	 std::cout << "Odd_and_even_work_as_expected" << std::endl;
	 std::cout << "NOT COMPUTED" << std::endl;//inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUPERSET.txt", toto);
	 std::cout << "peterson" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/peterson_SUBSET.txt", titi);
	 std::cout << "peterson" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUPERSET.txt", toto);
	 std::cout << "slides_example" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/slides_example_SUBSET.txt", titi);
	 std::cout << "slides_example" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUPERSET.txt", toto);
	 std::cout << "Specal_factors_are_unique" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/Specal_factors_are_unique_SUBSET.txt", titi);
	 std::cout << "Specal_factors_are_unique" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUPERSET.txt", toto);
	 std::cout << "The_lazy_Ostrowski_representation_is_unique" << std::endl;
	 inclusion(toto, titi);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/The_lazy_Ostrowski_representation_is_unique_SUBSET.txt", titi);
	 std::cout << "The_lazy_Ostrowski_representation_is_unique" << std::endl;
	 inclusion(titi, toto);
	 delete toto;
	 delete titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");
}


void debug_test3() {
	Automaton* toto;
	Automaton* titi;

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 auto start = std::chrono::high_resolution_clock::now();

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUPERSET.txt", toto);
	 std::cout << "fischerV3" << std::endl;
	 std::cout << inclusion(toto, titi) << std::endl; // forklift: 229 seconds (TRUE) -- quak: 264 seconds (TRUE)
	 delete toto;
	 delete titi;

	 auto end = std::chrono::high_resolution_clock::now();
	 std::chrono::duration<double> elapsed = end - start;
	 std::cout << elapsed.count() << " seconds.\n";

	 printf("--------------------------------------------\n");

	 start = std::chrono::high_resolution_clock::now();

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/fischerV3_SUBSET.txt", titi);
	 std::cout << "fischerV3" << std::endl;
	 std::cout << inclusion(titi, toto) << std::endl; // forklift: 242 seconds (TRUE) -- quak: 167 seconds (TRUE)
	 delete toto;
	 delete titi;

	 end = std::chrono::high_resolution_clock::now();
	 elapsed = end - start;
	 std::cout << elapsed.count() << " seconds.\n";

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

	 start = std::chrono::high_resolution_clock::now();

	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUBSET.txt");
	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUPERSET.txt", toto);
	 std::cout << "Odd_and_even_work_as_expected" << std::endl;
	 std::cout << inclusion(toto, titi) << std::endl; // forklift: 2.7 seconds (TRUE) -- quak: 4.6 seconds (TRUE)
	 delete toto;
	 delete titi;

	 end = std::chrono::high_resolution_clock::now();
	 elapsed = end - start;
	 std::cout << elapsed.count() << " seconds.\n";

	 printf("--------------------------------------------\n");

	 start = std::chrono::high_resolution_clock::now();

	 titi = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUPERSET.txt");
	 toto = Automaton::from_file_sync_alphabet("../samples/boolean/Odd_and_even_work_as_expected_SUBSET.txt", titi);
	 std::cout << "Odd_and_even_work_as_expected" << std::endl;
	 std::cout << inclusion(titi, toto) << std::endl; // forklift: 0.6 seconds (FALSE) -- quak: 1 seconds (FALSE)
	 delete toto;
	 delete titi;

	 end = std::chrono::high_resolution_clock::now();
	 elapsed = end - start;
	 std::cout << elapsed.count() << " seconds.\n";

	 printf("--------------------------------------------\n");
	 printf("--------------------------------------------\n");

}
