/*
 * Scores-> storing score of 6 houses
 * Questions-> stores question, option, img, and answers 
 * Control-> stores which question to display
 */
Scores = new Mongo.Collection('scores');
Questions = new Mongo.Collection('questions');
Control = new Mongo.Collection('display_ctl');

// client code
if (Meteor.isClient) {

	/*
	 * obtain scores, which question to display and hide the img panel by default
	 */
	Meteor.subscribe('theScores');		// subscribe to provide info to score panel
	Meteor.subscribe('display_ctl');	// subscribe to collection with status of what and what not to display
	Session.set("imgState", false);		// session variable to provide info: hide to angular
	Session.set("optionState", false);	// hide option panels if no option exist

	/*
	 * score board helpers
	 */
	Template.score_board.helpers({
		'A_score': function() {
			return Scores.findOne({ house: 'A' });
		},
		'D_score': function() {
			return Scores.findOne({ house: 'D' });
		},
		'H_score': function() {
			return Scores.findOne({ house: 'H' });
		},
		'J_score': function() {
			return Scores.findOne({ house: 'J' });
		},
		'L_score': function() {
			return Scores.findOne({ house: 'L' });
		},
		'M_score': function() {
			return Scores.findOne({ house: 'M' });
		}
	});

	/*
	 * question panel helpers
	 */
	Template.question_board.helpers({
		'question': function() {
			// obtain question id of question set be display now and subscribe
			var current_qid = Control.findOne({ control_id: 0 });
			console.log(current_qid.question_id);
			Meteor.subscribe('theQuestions', current_qid.question_id);

			// load question from db
			var question = Questions.findOne();

			// check if the program has image attached, if not hide image panel
			if(question.path==null)
				Session.set("imgState", false);	/* reactive */
			else
				Session.set("imgState", true);

			// disable option boxes if question is not MC
			if(question.optionA==null)
				Session.set("optionState", false);
			else
				Session.set("optionState", true);

			return question;
		},
		'img_state': function() {
			var image_state = Session.get("imgState");
			if(image_state==true) {
				return "";
			}
			else if(image_state==false) {
				return "hidden";
			}
		},
		'option': function() {
			var option_state = Session.get("optionState");
			if(option_state==true) {
				return "";
			}
			else if(option_state==false) {
				return "hidden";
			}
		},
		'answer': function() {
			var state = Control.findOne({ control_id: 1 });
			if(state.answer_status==true) {
				return "";
			}
			else if(state.answer_status==false) {
				return "hidden";
			}
		}
	});


	Router.onBeforeAction(function() {
		if(!Meteor.userId()) {
			return this.render('signin');
		}
		else {
			this.next();
		}
	});
	Router.map(function() {
		console.log(Meteor.user());
		this.route('game', { path: '/' });
		this.route('management', { path: '/back-stage' });
	});
}

// server code
if (Meteor.isServer) {
	Meteor.startup(function () {

		// drop collections to reset
		Scores.remove({});
		Questions.remove({});
		Control.remove({});

		// initialize scores
		Scores.insert({ house: 'A', score: 0 });
		Scores.insert({ house: 'D', score: 0 });
		Scores.insert({ house: 'H', score: 0 });
		Scores.insert({ house: 'J', score: 0 });
		Scores.insert({ house: 'L', score: 0 });
		Scores.insert({ house: 'M', score: 0 });

		// initialize display controller
		Control.insert({ control_id: 0, question_id: { catalog: "A", group: 1, qid: 1 } });
		Control.insert({ control_id: 1, answer_status: false });

		// initialze questions
		Questions.insert({ Id: { catalog: "A", group: 1, qid: 1 }, content: "content", optionA: "Option A", optionB: "Option B", optionC: "Option C", optionD: "Option D", correct: "Correct Answer" });
		Questions.insert({ Id: { catalog: "B", group: 2, qid: 1 }, content: "content2", optionA: "Option A2", optionB: "Option B", optionC: "Option C", optionD: "Option D", correct: "Correct Answer2" });
		Questions.insert({ Id: { catalog: "C", group: 1, qid: 1 }, content: "content", path: "imgs/B_2_3.png", correct: "Correct Answer" });

	});

	/*
	 * publish scores and question id of question to be displayed
	 */
	Meteor.publish('display_ctl', function() {
		return Control.find();
	});

	Meteor.publish('theScores', function() {
		return Scores.find({},{ house: 1, score: 1 });
	});

	Meteor.publish('theQuestions', function(qid) {
		var current_question = Control.findOne({ control_id: 0 }).question_id;
		if(current_question.catalog==qid.catalog && current_question.group==qid.group && current_question.qid==qid.qid) {
			Control.update({ control_id: 1 },{ $set: { answer_status: false } });
			return Questions.find({ Id: qid });
		}
	});
}
